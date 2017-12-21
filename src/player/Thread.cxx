/*
 * Copyright 2003-2017 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "Thread.hxx"
#include "Listener.hxx"
#include "decoder/DecoderThread.hxx"
#include "decoder/DecoderControl.hxx"
#include "MusicPipe.hxx"
#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"
#include "pcm/Silence.hxx"
#include "DetachedSong.hxx"
#include "CrossFade.hxx"
#include "Control.hxx"
#include "output/MultipleOutputs.hxx"
#include "tag/Tag.hxx"
#include "Idle.hxx"
#include "system/PeriodClock.hxx"
#include "util/Domain.hxx"
#include "thread/Name.hxx"
#include "Log.hxx"

#include <exception>

#include <string.h>

static constexpr Domain player_domain("player");

class Player {
	PlayerControl &pc;

	DecoderControl &dc;

	MusicBuffer &buffer;

	MusicPipe *pipe;

	/**
	 * the song currently being played
	 */
	std::unique_ptr<DetachedSong> song;

	/**
	 * The tag of the "next" song during cross-fade.  It is
	 * postponed, and sent to the output thread when the new song
	 * really begins.
	 */
	std::unique_ptr<Tag> cross_fade_tag;

	/**
	 * are we waiting for buffered_before_play?
	 */
	bool buffering = true;

	/**
	 * true if the decoder is starting and did not provide data
	 * yet
	 */
	bool decoder_starting = false;

	/**
	 * Did we wake up the DecoderThread recently?  This avoids
	 * duplicate wakeup calls.
	 */
	bool decoder_woken = false;

	/**
	 * is the player paused?
	 */
	bool paused = false;

	/**
	 * is there a new song in pc.next_song?
	 */
	bool queued = true;

	/**
	 * Was any audio output opened successfully?  It might have
	 * failed meanwhile, but was not explicitly closed by the
	 * player thread.  When this flag is unset, some output
	 * methods must not be called.
	 */
	bool output_open = false;

	/**
	 * Is cross-fading to the next song enabled?
	 */
	enum class CrossFadeState : uint8_t {
		/**
		 * The initial state: we don't know yet if we will
		 * cross-fade; it will be determined soon.
		 */
		UNKNOWN,

		/**
		 * Cross-fading is disabled for the transition to the
		 * next song.
		 */
		DISABLED,

		/**
		 * Cross-fading is enabled (but may not yet be in
		 * progress), will start near the end of the current
		 * song.
		 */
		ENABLED,

		/**
		 * Currently cross-fading to the next song.
		 */
		ACTIVE,
	} xfade_state = CrossFadeState::UNKNOWN;

	/**
	 * The number of chunks used for crossfading.
	 */
	unsigned cross_fade_chunks = 0;

	/**
	 * The current audio format for the audio outputs.
	 */
	AudioFormat play_audio_format = AudioFormat::Undefined();

	/**
	 * The time stamp of the chunk most recently sent to the
	 * output thread.  This attribute is only used if
	 * MultipleOutputs::GetElapsedTime() didn't return a usable
	 * value; the output thread can estimate the elapsed time more
	 * precisely.
	 */
	SongTime elapsed_time = SongTime::zero();

	PeriodClock throttle_silence_log;

public:
	Player(PlayerControl &_pc, DecoderControl &_dc,
	       MusicBuffer &_buffer) noexcept
		:pc(_pc), dc(_dc), buffer(_buffer) {}

private:
	/**
	 * Reset cross-fading to the initial state.  A check to
	 * re-enable it at an appropriate time will be scheduled.
	 */
	void ResetCrossFade() noexcept {
		xfade_state = CrossFadeState::UNKNOWN;
	}

	void ClearAndDeletePipe() noexcept {
		pipe->Clear(buffer);
		delete pipe;
	}

	void ClearAndReplacePipe(MusicPipe *_pipe) noexcept {
		ResetCrossFade();
		ClearAndDeletePipe();
		pipe = _pipe;
	}

	void ReplacePipe(MusicPipe *_pipe) noexcept {
		ResetCrossFade();
		delete pipe;
		pipe = _pipe;
	}

	/**
	 * Start the decoder.
	 *
	 * Player lock is not held.
	 */
	void StartDecoder(MusicPipe &pipe) noexcept;

	/**
	 * The decoder has acknowledged the "START" command (see
	 * ActivateDecoder()).  This function checks if the decoder
	 * initialization has completed yet.  If not, it will wait
	 * some more.
	 *
	 * Caller must lock the mutex.
	 *
	 * @return false if the decoder has failed, true on success
	 * (though the decoder startup may or may not yet be finished)
	 */
	bool CheckDecoderStartup() noexcept;

	/**
	 * Call CheckDecoderStartup() repeatedly until the decoder has
	 * finished startup.  Returns false on decoder error (and
	 * finishes the #PlayerCommand).
	 *
	 * This method does not check for commands.  It is only
	 * allowed to be used while a command is being handled.
	 *
	 * Caller must lock the mutex.
	 *
	 * @return false if the decoder has failed
	 */
	bool WaitDecoderStartup() noexcept {
		while (decoder_starting) {
			if (!CheckDecoderStartup()) {
				/* if decoder startup fails, make sure
				   the previous song is not being
				   played anymore */
				{
					const ScopeUnlock unlock(pc.mutex);
					pc.outputs.Cancel();
				}

				pc.CommandFinished();
				return false;
			}
		}

		return true;
	}

	bool LockWaitDecoderStartup() noexcept {
		const std::lock_guard<Mutex> lock(pc.mutex);
		return WaitDecoderStartup();
	}

	/**
	 * Stop the decoder and clears (and frees) its music pipe.
	 *
	 * Player lock is not held.
	 */
	void StopDecoder() noexcept;

	/**
	 * Is the decoder still busy on the same song as the player?
	 *
	 * Note: this function does not check if the decoder is already
	 * finished.
	 */
	gcc_pure
	bool IsDecoderAtCurrentSong() const noexcept {
		assert(pipe != nullptr);

		return dc.pipe == pipe;
	}

	/**
	 * Returns true if the decoder is decoding the next song (or has begun
	 * decoding it, or has finished doing it), and the player hasn't
	 * switched to that song yet.
	 */
	gcc_pure
	bool IsDecoderAtNextSong() const noexcept {
		return dc.pipe != nullptr && !IsDecoderAtCurrentSong();
	}

	/**
	 * This is the handler for the #PlayerCommand::SEEK command.
	 *
	 * The player lock is not held.
	 *
	 * @return false if the decoder has failed
	 */
	bool SeekDecoder() noexcept;

	/**
	 * Check if the decoder has reported an error, and forward it
	 * to PlayerControl::SetError().
	 *
	 * @return false if an error has occurred
	 */
	bool ForwardDecoderError() noexcept;

	/**
	 * After the decoder has been started asynchronously, activate
	 * it for playback.  That is, make the currently decoded song
	 * active (assign it to #song), clear PlayerControl::next_song
	 * and #queued, initialize #elapsed_time, and set
	 * #decoder_starting.
	 *
	 * When returning, the decoder may not have completed startup
	 * yet, therefore we don't know the audio format yet.  To
	 * finish decoder startup, call CheckDecoderStartup().
	 *
	 * The player lock is not held.
	 */
	void ActivateDecoder() noexcept;

	/**
	 * Wrapper for MultipleOutputs::Open().  Upon failure, it
	 * pauses the player.
	 *
	 * Caller must lock the mutex.
	 *
	 * @return true on success
	 */
	bool OpenOutput() noexcept;

	/**
	 * Obtains the next chunk from the music pipe, optionally applies
	 * cross-fading, and sends it to all audio outputs.
	 *
	 * @return true on success, false on error (playback will be stopped)
	 */
	bool PlayNextChunk() noexcept;

	/**
	 * Sends a chunk of silence to the audio outputs.  This is
	 * called when there is not enough decoded data in the pipe
	 * yet, to prevent underruns in the hardware buffers.
	 *
	 * The player lock is not held.
	 *
	 * @return false on error
	 */
	bool SendSilence() noexcept;

	/**
	 * Player lock must be held before calling.
	 */
	void ProcessCommand() noexcept;

	/**
	 * This is called at the border between two songs: the audio output
	 * has consumed all chunks of the current song, and we should start
	 * sending chunks from the next one.
	 *
	 * The player lock is not held.
	 */
	void SongBorder() noexcept;

public:
	/*
	 * The main loop of the player thread, during playback.  This
	 * is basically a state machine, which multiplexes data
	 * between the decoder thread and the output threads.
	 */
	void Run() noexcept;
};

void
Player::StartDecoder(MusicPipe &_pipe) noexcept
{
	assert(queued || pc.command == PlayerCommand::SEEK);
	assert(pc.next_song != nullptr);

	{
		/* copy ReplayGain parameters to the decoder */
		const std::lock_guard<Mutex> protect(pc.mutex);
		dc.replay_gain_mode = pc.replay_gain_mode;
	}

	SongTime start_time = pc.next_song->GetStartTime() + pc.seek_time;

	dc.Start(std::make_unique<DetachedSong>(*pc.next_song),
		 start_time, pc.next_song->GetEndTime(),
		 buffer, _pipe);
}

void
Player::StopDecoder() noexcept
{
	const PlayerControl::ScopeOccupied occupied(pc);

	dc.Stop();

	if (dc.pipe != nullptr) {
		/* clear and free the decoder pipe */

		dc.pipe->Clear(buffer);

		if (dc.pipe != pipe)
			delete dc.pipe;

		dc.pipe = nullptr;

		/* just in case we've been cross-fading: cancel it
		   now, because we just deleted the new song's decoder
		   pipe */
		ResetCrossFade();
	}
}

bool
Player::ForwardDecoderError() noexcept
{
	try {
		dc.CheckRethrowError();
	} catch (...) {
		pc.SetError(PlayerError::DECODER, std::current_exception());
		return false;
	}

	return true;
}

void
Player::ActivateDecoder() noexcept
{
	assert(queued || pc.command == PlayerCommand::SEEK);
	assert(pc.next_song != nullptr);

	queued = false;

	{
		const std::lock_guard<Mutex> lock(pc.mutex);

		pc.ClearTaggedSong();

		song = std::exchange(pc.next_song, nullptr);

		elapsed_time = pc.seek_time;

		/* set the "starting" flag, which will be cleared by
		   player_check_decoder_startup() */
		decoder_starting = true;

		/* update PlayerControl's song information */
		pc.total_time = song->GetDuration();
		pc.bit_rate = 0;
		pc.audio_format.Clear();
	}

	/* call syncPlaylistWithQueue() in the main thread */
	pc.listener.OnPlayerSync();
}

/**
 * Returns the real duration of the song, comprising the duration
 * indicated by the decoder plugin.
 */
static SignedSongTime
real_song_duration(const DetachedSong &song,
		   SignedSongTime decoder_duration) noexcept
{
	if (decoder_duration.IsNegative())
		/* the decoder plugin didn't provide information; fall
		   back to Song::GetDuration() */
		return song.GetDuration();

	const SongTime start_time = song.GetStartTime();
	const SongTime end_time = song.GetEndTime();

	if (end_time.IsPositive() && end_time < SongTime(decoder_duration))
		return SignedSongTime(end_time - start_time);

	return SignedSongTime(SongTime(decoder_duration) - start_time);
}

bool
Player::OpenOutput() noexcept
{
	assert(play_audio_format.IsDefined());
	assert(pc.state == PlayerState::PLAY ||
	       pc.state == PlayerState::PAUSE);

	try {
		const ScopeUnlock unlock(pc.mutex);
		pc.outputs.Open(play_audio_format, buffer);
	} catch (...) {
		LogError(std::current_exception());

		output_open = false;

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		paused = true;

		pc.SetOutputError(std::current_exception());

		idle_add(IDLE_PLAYER);

		return false;
	}

	output_open = true;
	paused = false;

	pc.state = PlayerState::PLAY;

	idle_add(IDLE_PLAYER);

	return true;
}

bool
Player::CheckDecoderStartup() noexcept
{
	assert(decoder_starting);

	if (!ForwardDecoderError()) {
		/* the decoder failed */
		return false;
	} else if (!dc.IsStarting()) {
		/* the decoder is ready and ok */

		if (output_open &&
		    !pc.WaitOutputConsumed(1))
			/* the output devices havn't finished playing
			   all chunks yet - wait for that */
			return true;

		pc.total_time = real_song_duration(*dc.song,
						   dc.total_time);
		pc.audio_format = dc.in_audio_format;
		play_audio_format = dc.out_audio_format;
		decoder_starting = false;

		idle_add(IDLE_PLAYER);

		if (!paused && !OpenOutput()) {
			FormatError(player_domain,
				    "problems opening audio device "
				    "while playing \"%s\"",
				    dc.song->GetURI());
			return true;
		}

		return true;
	} else {
		/* the decoder is not yet ready; wait
		   some more */
		dc.WaitForDecoder();

		return true;
	}
}

bool
Player::SendSilence() noexcept
{
	assert(output_open);
	assert(play_audio_format.IsDefined());

	MusicChunk *chunk = buffer.Allocate();
	if (chunk == nullptr) {
		/* this is non-fatal, because this means that the
		   decoder has filled to buffer completely meanwhile;
		   by ignoring the error, we work around this race
		   condition */
		LogDebug(player_domain, "Failed to allocate silence buffer");
		return true;
	}

#ifndef NDEBUG
	chunk->audio_format = play_audio_format;
#endif

	const size_t frame_size = play_audio_format.GetFrameSize();
	/* this formula ensures that we don't send
	   partial frames */
	unsigned num_frames = sizeof(chunk->data) / frame_size;

	chunk->bit_rate = 0;
	chunk->time = SignedSongTime::Negative(); /* undefined time stamp */
	chunk->length = num_frames * frame_size;
	chunk->replay_gain_serial = MusicChunk::IGNORE_REPLAY_GAIN;
	PcmSilence({chunk->data, chunk->length}, play_audio_format.format);

	try {
		pc.outputs.Play(chunk);
	} catch (...) {
		LogError(std::current_exception());
		buffer.Return(chunk);
		return false;
	}

	return true;
}

inline bool
Player::SeekDecoder() noexcept
{
	assert(pc.next_song != nullptr);

	pc.outputs.Cancel();

	if (!dc.LockIsCurrentSong(*pc.next_song)) {
		/* the decoder is already decoding the "next" song -
		   stop it and start the previous song again */

		StopDecoder();

		/* clear music chunks which might still reside in the
		   pipe */
		pipe->Clear(buffer);

		/* re-start the decoder */
		StartDecoder(*pipe);
		ActivateDecoder();

		if (!LockWaitDecoderStartup())
			return false;
	} else {
		if (!IsDecoderAtCurrentSong()) {
			/* the decoder is already decoding the "next" song,
			   but it is the same song file; exchange the pipe */
			ClearAndReplacePipe(dc.pipe);
		}

		const std::lock_guard<Mutex> lock(pc.mutex);

		const SongTime start_time = pc.next_song->GetStartTime();
		pc.next_song.reset();
		queued = false;

		/* wait for the decoder to complete initialization
		   (just in case that happens to be still in
		   progress) */

		if (!WaitDecoderStartup())
			return false;

		/* send the SEEK command */

		SongTime where = pc.seek_time;
		if (!pc.total_time.IsNegative()) {
			const SongTime total_time(pc.total_time);
			if (where > total_time)
				where = total_time;
		}

		try {
			const PlayerControl::ScopeOccupied occupied(pc);

			dc.Seek(where + start_time);
		} catch (...) {
			/* decoder failure */
			pc.SetError(PlayerError::DECODER,
				    std::current_exception());
			pc.CommandFinished();
			return false;
		}

		elapsed_time = where;
	}

	pc.LockCommandFinished();

	assert(xfade_state == CrossFadeState::UNKNOWN);

	/* re-fill the buffer after seeking */
	buffering = true;

	return true;
}

inline void
Player::ProcessCommand() noexcept
{
	switch (pc.command) {
	case PlayerCommand::NONE:
	case PlayerCommand::STOP:
	case PlayerCommand::EXIT:
	case PlayerCommand::CLOSE_AUDIO:
		break;

	case PlayerCommand::UPDATE_AUDIO:
		{
			const ScopeUnlock unlock(pc.mutex);
			pc.outputs.EnableDisable();
		}

		pc.CommandFinished();
		break;

	case PlayerCommand::QUEUE:
		assert(pc.next_song != nullptr);
		assert(!queued);
		assert(!IsDecoderAtNextSong());

		queued = true;
		pc.CommandFinished();

		{
			const ScopeUnlock unlock(pc.mutex);
			if (dc.LockIsIdle())
				StartDecoder(*new MusicPipe());
		}

		break;

	case PlayerCommand::PAUSE:
		paused = !paused;
		if (paused) {
			pc.state = PlayerState::PAUSE;

			const ScopeUnlock unlock(pc.mutex);
			pc.outputs.Pause();
		} else if (!play_audio_format.IsDefined()) {
			/* the decoder hasn't provided an audio format
			   yet - don't open the audio device yet */
			pc.state = PlayerState::PLAY;
		} else {
			OpenOutput();
		}

		pc.CommandFinished();
		break;

	case PlayerCommand::SEEK:
		{
			const ScopeUnlock unlock(pc.mutex);
			SeekDecoder();
		}
		break;

	case PlayerCommand::CANCEL:
		if (pc.next_song == nullptr) {
			/* the cancel request arrived too late, we're
			   already playing the queued song...  stop
			   everything now */
			pc.command = PlayerCommand::STOP;
			return;
		}

		if (IsDecoderAtNextSong()) {
			/* the decoder is already decoding the song -
			   stop it and reset the position */
			const ScopeUnlock unlock(pc.mutex);
			StopDecoder();
		}

		pc.next_song.reset();
		queued = false;
		pc.CommandFinished();
		break;

	case PlayerCommand::REFRESH:
		if (output_open && !paused) {
			const ScopeUnlock unlock(pc.mutex);
			pc.outputs.Check();
		}

		pc.elapsed_time = !pc.outputs.GetElapsedTime().IsNegative()
			? SongTime(pc.outputs.GetElapsedTime())
			: elapsed_time;

		pc.CommandFinished();
		break;
	}
}

static void
update_song_tag(PlayerControl &pc, DetachedSong &song,
		const Tag &new_tag) noexcept
{
	if (song.IsFile())
		/* don't update tags of local files, only remote
		   streams may change tags dynamically */
		return;

	song.SetTag(new_tag);

	pc.LockSetTaggedSong(song);

	/* the main thread will update the playlist version when he
	   receives this event */
	pc.listener.OnPlayerTagModified();

	/* notify all clients that the tag of the current song has
	   changed */
	idle_add(IDLE_PLAYER);
}

/**
 * Plays a #MusicChunk object (after applying software volume).  If
 * it contains a (stream) tag, copy it to the current song, so MPD's
 * playlist reflects the new stream tag.
 *
 * Player lock is not held.
 */
static void
play_chunk(PlayerControl &pc,
	   DetachedSong &song, MusicChunk *chunk,
	   MusicBuffer &buffer,
	   const AudioFormat format) noexcept
{
	assert(chunk->CheckFormat(format));

	if (chunk->tag != nullptr)
		update_song_tag(pc, song, *chunk->tag);

	if (chunk->IsEmpty()) {
		buffer.Return(chunk);
		return;
	}

	{
		const std::lock_guard<Mutex> lock(pc.mutex);
		pc.bit_rate = chunk->bit_rate;
	}

	/* send the chunk to the audio outputs */

	pc.outputs.Play(chunk);
	pc.total_play_time += (double)chunk->length /
		format.GetTimeToSize();
}

inline bool
Player::PlayNextChunk() noexcept
{
	if (!pc.LockWaitOutputConsumed(64))
		/* the output pipe is still large enough, don't send
		   another chunk */
		return true;

	/* activate cross-fading? */
	if (xfade_state == CrossFadeState::ENABLED &&
	    IsDecoderAtNextSong() &&
	    pipe->GetSize() <= cross_fade_chunks) {
		/* beginning of the cross fade - adjust
		   cross_fade_chunks which might be bigger than the
		   remaining number of chunks in the old song */
		cross_fade_chunks = pipe->GetSize();
		xfade_state = CrossFadeState::ACTIVE;
	}

	MusicChunk *chunk = nullptr;
	if (xfade_state == CrossFadeState::ACTIVE) {
		/* perform cross fade */

		assert(IsDecoderAtNextSong());

		unsigned cross_fade_position = pipe->GetSize();
		assert(cross_fade_position <= cross_fade_chunks);

		MusicChunk *other_chunk = dc.pipe->Shift();
		if (other_chunk != nullptr) {
			chunk = pipe->Shift();
			assert(chunk != nullptr);
			assert(chunk->other == nullptr);

			/* don't send the tags of the new song (which
			   is being faded in) yet; postpone it until
			   the current song is faded out */
			cross_fade_tag = Tag::Merge(std::move(cross_fade_tag),
						    std::move(other_chunk->tag));

			if (pc.cross_fade.mixramp_delay <= 0) {
				chunk->mix_ratio = ((float)cross_fade_position)
					     / cross_fade_chunks;
			} else {
				chunk->mix_ratio = -1;
			}

			if (other_chunk->IsEmpty()) {
				/* the "other" chunk was a MusicChunk
				   which had only a tag, but no music
				   data - we cannot cross-fade that;
				   but since this happens only at the
				   beginning of the new song, we can
				   easily recover by throwing it away
				   now */
				buffer.Return(other_chunk);
				other_chunk = nullptr;
			}

			chunk->other = other_chunk;
		} else {
			/* there are not enough decoded chunks yet */

			const std::lock_guard<Mutex> lock(pc.mutex);

			if (dc.IsIdle()) {
				/* the decoder isn't running, abort
				   cross fading */
				xfade_state = CrossFadeState::DISABLED;
			} else {
				/* wait for the decoder */
				dc.Signal();
				dc.WaitForDecoder();

				return true;
			}
		}
	}

	if (chunk == nullptr)
		chunk = pipe->Shift();

	assert(chunk != nullptr);

	/* insert the postponed tag if cross-fading is finished */

	if (xfade_state != CrossFadeState::ACTIVE && cross_fade_tag != nullptr) {
		chunk->tag = Tag::Merge(std::move(chunk->tag),
					std::move(cross_fade_tag));
		cross_fade_tag = nullptr;
	}

	/* play the current chunk */

	try {
		play_chunk(pc, *song, chunk, buffer, play_audio_format);
	} catch (...) {
		LogError(std::current_exception());

		buffer.Return(chunk);

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		paused = true;

		pc.LockSetOutputError(std::current_exception());

		idle_add(IDLE_PLAYER);

		return false;
	}

	const std::lock_guard<Mutex> lock(pc.mutex);

	/* this formula should prevent that the decoder gets woken up
	   with each chunk; it is more efficient to make it decode a
	   larger block at a time */
	if (!dc.IsIdle() &&
	    dc.pipe->GetSize() <= (pc.buffered_before_play +
				   buffer.GetSize() * 3) / 4) {
		if (!decoder_woken) {
			decoder_woken = true;
			dc.Signal();
		}
	} else
		decoder_woken = false;

	return true;
}

inline void
Player::SongBorder() noexcept
{
	FormatDefault(player_domain, "played \"%s\"", song->GetURI());

	throttle_silence_log.Reset();

	ReplacePipe(dc.pipe);

	pc.outputs.SongBorder();

	ActivateDecoder();

	const bool border_pause = pc.LockApplyBorderPause();
	if (border_pause) {
		paused = true;
		idle_add(IDLE_PLAYER);
	}
}

inline void
Player::Run() noexcept
{
	pipe = new MusicPipe();

	StartDecoder(*pipe);
	ActivateDecoder();

	pc.Lock();
	pc.state = PlayerState::PLAY;

	pc.CommandFinished();

	while (true) {
		ProcessCommand();
		if (pc.command == PlayerCommand::STOP ||
		    pc.command == PlayerCommand::EXIT ||
		    pc.command == PlayerCommand::CLOSE_AUDIO) {
			pc.Unlock();
			pc.outputs.Cancel();
			break;
		}

		pc.Unlock();

		if (buffering) {
			/* buffering at the start of the song - wait
			   until the buffer is large enough, to
			   prevent stuttering on slow machines */

			if (pipe->GetSize() < pc.buffered_before_play &&
			    !dc.LockIsIdle()) {
				/* not enough decoded buffer space yet */

				if (!paused && output_open &&
				    pc.outputs.Check() < 4 &&
				    !SendSilence())
					break;

				pc.Lock();
				/* XXX race condition: check decoder again */
				dc.WaitForDecoder();
				continue;
			} else {
				/* buffering is complete */
				buffering = false;
			}
		}

		if (decoder_starting) {
			/* wait until the decoder is initialized completely */

			pc.Lock();

			if (!CheckDecoderStartup()) {
				pc.Unlock();
				break;
			}

			continue;
		}

#ifndef NDEBUG
		/*
		music_pipe_check_format(&play_audio_format,
					next_song_chunk,
					&dc.out_audio_format);
		*/
#endif

		if (dc.LockIsIdle() && queued && dc.pipe == pipe) {
			/* the decoder has finished the current song;
			   make it decode the next song */

			assert(dc.pipe == nullptr || dc.pipe == pipe);

			StartDecoder(*new MusicPipe());
		}

		if (/* no cross-fading if MPD is going to pause at the
		       end of the current song */
		    !pc.border_pause &&
		    IsDecoderAtNextSong() &&
		    xfade_state == CrossFadeState::UNKNOWN &&
		    !dc.LockIsStarting()) {
			/* enable cross fading in this song?  if yes,
			   calculate how many chunks will be required
			   for it */
			cross_fade_chunks =
				pc.cross_fade.Calculate(dc.total_time,
							dc.replay_gain_db,
							dc.replay_gain_prev_db,
							dc.GetMixRampStart(),
							dc.GetMixRampPreviousEnd(),
							dc.out_audio_format,
							play_audio_format,
							buffer.GetSize() -
							pc.buffered_before_play);
			if (cross_fade_chunks > 0)
				xfade_state = CrossFadeState::ENABLED;
			else
				/* cross fading is disabled or the
				   next song is too short */
				xfade_state = CrossFadeState::DISABLED;
		}

		if (paused) {
			pc.Lock();

			if (pc.command == PlayerCommand::NONE)
				pc.Wait();
			continue;
		} else if (!pipe->IsEmpty()) {
			/* at least one music chunk is ready - send it
			   to the audio output */

			PlayNextChunk();
		} else if (pc.outputs.Check() > 0) {
			/* not enough data from decoder, but the
			   output thread is still busy, so it's
			   okay */

			pc.Lock();

			/* wake up the decoder (just in case it's
			   waiting for space in the MusicBuffer) and
			   wait for it */
			dc.Signal();
			dc.WaitForDecoder();
			continue;
		} else if (IsDecoderAtNextSong()) {
			/* at the beginning of a new song */

			SongBorder();
		} else if (dc.LockIsIdle()) {
			/* check the size of the pipe again, because
			   the decoder thread may have added something
			   since we last checked */
			if (pipe->IsEmpty()) {
				/* wait for the hardware to finish
				   playback */
				pc.outputs.Drain();
				break;
			}
		} else if (output_open) {
			/* the decoder is too busy and hasn't provided
			   new PCM data in time: send silence (if the
			   output pipe is empty) */

			if (throttle_silence_log.CheckUpdate(std::chrono::seconds(5)))
				FormatWarning(player_domain, "Decoder is too slow; playing silence to avoid xrun");

			if (!SendSilence())
				break;
		}

		pc.Lock();
	}

	StopDecoder();

	ClearAndDeletePipe();

	cross_fade_tag.reset();

	if (song != nullptr) {
		FormatDefault(player_domain, "played \"%s\"", song->GetURI());
		song.reset();
	}

	const std::lock_guard<Mutex> lock(pc.mutex);

	pc.ClearTaggedSong();

	if (queued) {
		assert(pc.next_song != nullptr);
		pc.next_song.reset();
	}

	pc.state = PlayerState::STOP;
}

static void
do_play(PlayerControl &pc, DecoderControl &dc,
	MusicBuffer &buffer) noexcept
{
	Player player(pc, dc, buffer);
	player.Run();
}

void
PlayerControl::RunThread() noexcept
{
	SetThreadName("player");

	DecoderControl dc(mutex, cond,
			  configured_audio_format,
			  replay_gain_config);
	decoder_thread_start(dc);

	MusicBuffer buffer(buffer_chunks);

	Lock();

	while (1) {
		switch (command) {
		case PlayerCommand::SEEK:
		case PlayerCommand::QUEUE:
			assert(next_song != nullptr);

			Unlock();
			do_play(*this, dc, buffer);
			listener.OnPlayerSync();
			Lock();
			break;

		case PlayerCommand::STOP:
			Unlock();
			outputs.Cancel();
			Lock();

			/* fall through */

		case PlayerCommand::PAUSE:
			next_song.reset();

			CommandFinished();
			break;

		case PlayerCommand::CLOSE_AUDIO:
			Unlock();

			outputs.Release();

			Lock();
			CommandFinished();

			assert(buffer.IsEmptyUnsafe());

			break;

		case PlayerCommand::UPDATE_AUDIO:
			Unlock();
			outputs.EnableDisable();
			Lock();
			CommandFinished();
			break;

		case PlayerCommand::EXIT:
			Unlock();

			dc.Quit();

			outputs.Close();

			LockCommandFinished();
			return;

		case PlayerCommand::CANCEL:
			next_song.reset();

			CommandFinished();
			break;

		case PlayerCommand::REFRESH:
			/* no-op when not playing */
			CommandFinished();
			break;

		case PlayerCommand::NONE:
			Wait();
			break;
		}
	}
}

void
StartPlayerThread(PlayerControl &pc)
{
	assert(!pc.thread.IsDefined());

	pc.thread.Start();
}
