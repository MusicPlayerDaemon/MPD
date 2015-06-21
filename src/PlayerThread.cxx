/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "PlayerThread.hxx"
#include "PlayerListener.hxx"
#include "decoder/DecoderThread.hxx"
#include "decoder/DecoderControl.hxx"
#include "MusicPipe.hxx"
#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"
#include "DetachedSong.hxx"
#include "system/FatalError.hxx"
#include "CrossFade.hxx"
#include "PlayerControl.hxx"
#include "output/MultipleOutputs.hxx"
#include "tag/Tag.hxx"
#include "Idle.hxx"
#include "util/Domain.hxx"
#include "thread/Name.hxx"
#include "Log.hxx"

#include <string.h>

static constexpr Domain player_domain("player");

enum class CrossFadeState : int8_t {
	DISABLED = -1,
	UNKNOWN = 0,
	ENABLED = 1
};

class Player {
	PlayerControl &pc;

	DecoderControl &dc;

	MusicBuffer &buffer;

	MusicPipe *pipe;

	/**
	 * are we waiting for buffered_before_play?
	 */
	bool buffering;

	/**
	 * true if the decoder is starting and did not provide data
	 * yet
	 */
	bool decoder_starting;

	/**
	 * Did we wake up the DecoderThread recently?  This avoids
	 * duplicate wakeup calls.
	 */
	bool decoder_woken;

	/**
	 * is the player paused?
	 */
	bool paused;

	/**
	 * is there a new song in pc.next_song?
	 */
	bool queued;

	/**
	 * Was any audio output opened successfully?  It might have
	 * failed meanwhile, but was not explicitly closed by the
	 * player thread.  When this flag is unset, some output
	 * methods must not be called.
	 */
	bool output_open;

	/**
	 * the song currently being played
	 */
	DetachedSong *song;

	/**
	 * is cross fading enabled?
	 */
	CrossFadeState xfade_state;

	/**
	 * has cross-fading begun?
	 */
	bool cross_fading;

	/**
	 * The number of chunks used for crossfading.
	 */
	unsigned cross_fade_chunks;

	/**
	 * The tag of the "next" song during cross-fade.  It is
	 * postponed, and sent to the output thread when the new song
	 * really begins.
	 */
	Tag *cross_fade_tag;

	/**
	 * The current audio format for the audio outputs.
	 */
	AudioFormat play_audio_format;

	/**
	 * The time stamp of the chunk most recently sent to the
	 * output thread.  This attribute is only used if
	 * MultipleOutputs::GetElapsedTime() didn't return a usable
	 * value; the output thread can estimate the elapsed time more
	 * precisely.
	 */
	SongTime elapsed_time;

public:
	Player(PlayerControl &_pc, DecoderControl &_dc,
	       MusicBuffer &_buffer)
		:pc(_pc), dc(_dc), buffer(_buffer),
		 buffering(true),
		 decoder_starting(false),
		 decoder_woken(false),
		 paused(false),
		 queued(true),
		 output_open(false),
		 song(nullptr),
		 xfade_state(CrossFadeState::UNKNOWN),
		 cross_fading(false),
		 cross_fade_chunks(0),
		 cross_fade_tag(nullptr),
		 elapsed_time(SongTime::zero()) {}

private:
	void ClearAndDeletePipe() {
		pipe->Clear(buffer);
		delete pipe;
	}

	void ClearAndReplacePipe(MusicPipe *_pipe) {
		ClearAndDeletePipe();
		pipe = _pipe;
	}

	void ReplacePipe(MusicPipe *_pipe) {
		delete pipe;
		pipe = _pipe;
	}

	/**
	 * Start the decoder.
	 *
	 * Player lock is not held.
	 */
	void StartDecoder(MusicPipe &pipe);

	/**
	 * The decoder has acknowledged the "START" command (see
	 * player::WaitForDecoder()).  This function checks if the decoder
	 * initialization has completed yet.
	 *
	 * The player lock is not held.
	 */
	bool CheckDecoderStartup();

	/**
	 * Stop the decoder and clears (and frees) its music pipe.
	 *
	 * Player lock is not held.
	 */
	void StopDecoder();

	/**
	 * Is the decoder still busy on the same song as the player?
	 *
	 * Note: this function does not check if the decoder is already
	 * finished.
	 */
	gcc_pure
	bool IsDecoderAtCurrentSong() const {
		assert(pipe != nullptr);

		return dc.pipe == pipe;
	}

	/**
	 * Returns true if the decoder is decoding the next song (or has begun
	 * decoding it, or has finished doing it), and the player hasn't
	 * switched to that song yet.
	 */
	gcc_pure
	bool IsDecoderAtNextSong() const {
		return dc.pipe != nullptr && !IsDecoderAtCurrentSong();
	}

	/**
	 * This is the handler for the #PlayerCommand::SEEK command.
	 *
	 * The player lock is not held.
	 */
	bool SeekDecoder();

	/**
	 * After the decoder has been started asynchronously, wait for
	 * the "START" command to finish.  The decoder may not be
	 * initialized yet, i.e. there is no audio_format information
	 * yet.
	 *
	 * The player lock is not held.
	 */
	bool WaitForDecoder();

	/**
	 * Wrapper for MultipleOutputs::Open().  Upon failure, it
	 * pauses the player.
	 *
	 * @return true on success
	 */
	bool OpenOutput();

	/**
	 * Obtains the next chunk from the music pipe, optionally applies
	 * cross-fading, and sends it to all audio outputs.
	 *
	 * @return true on success, false on error (playback will be stopped)
	 */
	bool PlayNextChunk();

	/**
	 * Sends a chunk of silence to the audio outputs.  This is
	 * called when there is not enough decoded data in the pipe
	 * yet, to prevent underruns in the hardware buffers.
	 *
	 * The player lock is not held.
	 */
	bool SendSilence();

	/**
	 * Player lock must be held before calling.
	 */
	void ProcessCommand();

	/**
	 * This is called at the border between two songs: the audio output
	 * has consumed all chunks of the current song, and we should start
	 * sending chunks from the next one.
	 *
	 * The player lock is not held.
	 *
	 * @return true on success, false on error (playback will be stopped)
	 */
	bool SongBorder();

public:
	/*
	 * The main loop of the player thread, during playback.  This
	 * is basically a state machine, which multiplexes data
	 * between the decoder thread and the output threads.
	 */
	void Run();
};

static void
player_command_finished(PlayerControl &pc)
{
	pc.Lock();
	pc.CommandFinished();
	pc.Unlock();
}

void
Player::StartDecoder(MusicPipe &_pipe)
{
	assert(queued || pc.command == PlayerCommand::SEEK);
	assert(pc.next_song != nullptr);

	SongTime start_time = pc.next_song->GetStartTime();
	if (pc.command == PlayerCommand::SEEK)
		start_time += pc.seek_time;

	dc.Start(new DetachedSong(*pc.next_song),
		 start_time, pc.next_song->GetEndTime(),
		 buffer, _pipe);
}

void
Player::StopDecoder()
{
	dc.Stop();

	if (dc.pipe != nullptr) {
		/* clear and free the decoder pipe */

		dc.pipe->Clear(buffer);

		if (dc.pipe != pipe)
			delete dc.pipe;

		dc.pipe = nullptr;
	}
}

bool
Player::WaitForDecoder()
{
	assert(queued || pc.command == PlayerCommand::SEEK);
	assert(pc.next_song != nullptr);

	queued = false;

	pc.Lock();
	Error error = dc.GetError();
	if (error.IsDefined()) {
		pc.SetError(PlayerError::DECODER, std::move(error));

		delete pc.next_song;
		pc.next_song = nullptr;

		pc.Unlock();

		return false;
	}

	pc.ClearTaggedSong();

	delete song;
	song = pc.next_song;
	elapsed_time = SongTime::zero();

	/* set the "starting" flag, which will be cleared by
	   player_check_decoder_startup() */
	decoder_starting = true;

	/* update PlayerControl's song information */
	pc.total_time = pc.next_song->GetDuration();
	pc.bit_rate = 0;
	pc.audio_format.Clear();

	/* clear the queued song */
	pc.next_song = nullptr;

	pc.Unlock();

	/* call syncPlaylistWithQueue() in the main thread */
	pc.listener.OnPlayerSync();

	return true;
}

/**
 * Returns the real duration of the song, comprising the duration
 * indicated by the decoder plugin.
 */
static SignedSongTime
real_song_duration(const DetachedSong &song, SignedSongTime decoder_duration)
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
Player::OpenOutput()
{
	assert(play_audio_format.IsDefined());
	assert(pc.state == PlayerState::PLAY ||
	       pc.state == PlayerState::PAUSE);

	Error error;
	if (pc.outputs.Open(play_audio_format, buffer, error)) {
		output_open = true;
		paused = false;

		pc.Lock();
		pc.state = PlayerState::PLAY;
		pc.Unlock();

		idle_add(IDLE_PLAYER);

		return true;
	} else {
		LogError(error);

		output_open = false;

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		paused = true;

		pc.Lock();
		pc.SetError(PlayerError::OUTPUT, std::move(error));
		pc.state = PlayerState::PAUSE;
		pc.Unlock();

		idle_add(IDLE_PLAYER);

		return false;
	}
}

bool
Player::CheckDecoderStartup()
{
	assert(decoder_starting);

	pc.Lock();

	Error error = dc.GetError();
	if (error.IsDefined()) {
		/* the decoder failed */
		pc.SetError(PlayerError::DECODER, std::move(error));
		pc.Unlock();

		return false;
	} else if (!dc.IsStarting()) {
		/* the decoder is ready and ok */

		pc.Unlock();

		if (output_open &&
		    !pc.outputs.Wait(pc, 1))
			/* the output devices havn't finished playing
			   all chunks yet - wait for that */
			return true;

		pc.Lock();
		pc.total_time = real_song_duration(*dc.song, dc.total_time);
		pc.audio_format = dc.in_audio_format;
		pc.Unlock();

		idle_add(IDLE_PLAYER);

		play_audio_format = dc.out_audio_format;
		decoder_starting = false;

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
		pc.Unlock();

		return true;
	}
}

bool
Player::SendSilence()
{
	assert(output_open);
	assert(play_audio_format.IsDefined());

	MusicChunk *chunk = buffer.Allocate();
	if (chunk == nullptr) {
		LogError(player_domain, "Failed to allocate silence buffer");
		return false;
	}

#ifndef NDEBUG
	chunk->audio_format = play_audio_format;
#endif

	const size_t frame_size = play_audio_format.GetFrameSize();
	/* this formula ensures that we don't send
	   partial frames */
	unsigned num_frames = sizeof(chunk->data) / frame_size;

	chunk->time = SignedSongTime::Negative(); /* undefined time stamp */
	chunk->length = num_frames * frame_size;
	memset(chunk->data, 0, chunk->length);

	Error error;
	if (!pc.outputs.Play(chunk, error)) {
		LogError(error);
		buffer.Return(chunk);
		return false;
	}

	return true;
}

inline bool
Player::SeekDecoder()
{
	assert(pc.next_song != nullptr);

	const SongTime start_time = pc.next_song->GetStartTime();

	if (!dc.LockIsCurrentSong(*pc.next_song)) {
		/* the decoder is already decoding the "next" song -
		   stop it and start the previous song again */

		StopDecoder();

		/* clear music chunks which might still reside in the
		   pipe */
		pipe->Clear(buffer);

		/* re-start the decoder */
		StartDecoder(*pipe);
		if (!WaitForDecoder()) {
			/* decoder failure */
			player_command_finished(pc);
			return false;
		}
	} else {
		if (!IsDecoderAtCurrentSong()) {
			/* the decoder is already decoding the "next" song,
			   but it is the same song file; exchange the pipe */
			ClearAndReplacePipe(dc.pipe);
		}

		delete pc.next_song;
		pc.next_song = nullptr;
		queued = false;
	}

	/* wait for the decoder to complete initialization */

	while (decoder_starting) {
		if (!CheckDecoderStartup()) {
			/* decoder failure */
			player_command_finished(pc);
			return false;
		}
	}

	/* send the SEEK command */

	SongTime where = pc.seek_time;
	if (!pc.total_time.IsNegative()) {
		const SongTime total_time(pc.total_time);
		if (where > total_time)
			where = total_time;
	}

	if (!dc.Seek(where + start_time)) {
		/* decoder failure */
		player_command_finished(pc);
		return false;
	}

	elapsed_time = where;

	player_command_finished(pc);

	xfade_state = CrossFadeState::UNKNOWN;

	/* re-fill the buffer after seeking */
	buffering = true;

	pc.outputs.Cancel();

	return true;
}

inline void
Player::ProcessCommand()
{
	switch (pc.command) {
	case PlayerCommand::NONE:
	case PlayerCommand::STOP:
	case PlayerCommand::EXIT:
	case PlayerCommand::CLOSE_AUDIO:
		break;

	case PlayerCommand::UPDATE_AUDIO:
		pc.Unlock();
		pc.outputs.EnableDisable();
		pc.Lock();
		pc.CommandFinished();
		break;

	case PlayerCommand::QUEUE:
		assert(pc.next_song != nullptr);
		assert(!queued);
		assert(!IsDecoderAtNextSong());

		queued = true;
		pc.CommandFinished();

		pc.Unlock();
		if (dc.LockIsIdle())
			StartDecoder(*new MusicPipe());
		pc.Lock();

		break;

	case PlayerCommand::PAUSE:
		pc.Unlock();

		paused = !paused;
		if (paused) {
			pc.outputs.Pause();
			pc.Lock();

			pc.state = PlayerState::PAUSE;
		} else if (!play_audio_format.IsDefined()) {
			/* the decoder hasn't provided an audio format
			   yet - don't open the audio device yet */
			pc.Lock();

			pc.state = PlayerState::PLAY;
		} else {
			OpenOutput();

			pc.Lock();
		}

		pc.CommandFinished();
		break;

	case PlayerCommand::SEEK:
		pc.Unlock();
		SeekDecoder();
		pc.Lock();
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
			pc.Unlock();
			StopDecoder();
			pc.Lock();
		}

		delete pc.next_song;
		pc.next_song = nullptr;
		queued = false;
		pc.CommandFinished();
		break;

	case PlayerCommand::REFRESH:
		if (output_open && !paused) {
			pc.Unlock();
			pc.outputs.Check();
			pc.Lock();
		}

		pc.elapsed_time = !pc.outputs.GetElapsedTime().IsNegative()
			? SongTime(pc.outputs.GetElapsedTime())
			: elapsed_time;

		pc.CommandFinished();
		break;
	}
}

static void
update_song_tag(PlayerControl &pc, DetachedSong &song, const Tag &new_tag)
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
static bool
play_chunk(PlayerControl &pc,
	   DetachedSong &song, MusicChunk *chunk,
	   MusicBuffer &buffer,
	   const AudioFormat format,
	   Error &error)
{
	assert(chunk->CheckFormat(format));

	if (chunk->tag != nullptr)
		update_song_tag(pc, song, *chunk->tag);

	if (chunk->IsEmpty()) {
		buffer.Return(chunk);
		return true;
	}

	pc.Lock();
	pc.bit_rate = chunk->bit_rate;
	pc.Unlock();

	/* send the chunk to the audio outputs */

	if (!pc.outputs.Play(chunk, error))
		return false;

	pc.total_play_time += (double)chunk->length /
		format.GetTimeToSize();
	return true;
}

inline bool
Player::PlayNextChunk()
{
	if (!pc.outputs.Wait(pc, 64))
		/* the output pipe is still large enough, don't send
		   another chunk */
		return true;

	unsigned cross_fade_position;
	MusicChunk *chunk = nullptr;
	if (xfade_state == CrossFadeState::ENABLED && IsDecoderAtNextSong() &&
	    (cross_fade_position = pipe->GetSize()) <= cross_fade_chunks) {
		/* perform cross fade */
		MusicChunk *other_chunk = dc.pipe->Shift();

		if (!cross_fading) {
			/* beginning of the cross fade - adjust
			   crossFadeChunks which might be bigger than
			   the remaining number of chunks in the old
			   song */
			cross_fade_chunks = cross_fade_position;
			cross_fading = true;
		}

		if (other_chunk != nullptr) {
			chunk = pipe->Shift();
			assert(chunk != nullptr);
			assert(chunk->other == nullptr);

			/* don't send the tags of the new song (which
			   is being faded in) yet; postpone it until
			   the current song is faded out */
			cross_fade_tag =
				Tag::MergeReplace(cross_fade_tag,
						  other_chunk->tag);
			other_chunk->tag = nullptr;

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

			pc.Lock();

			if (dc.IsIdle()) {
				/* the decoder isn't running, abort
				   cross fading */
				pc.Unlock();

				xfade_state = CrossFadeState::DISABLED;
			} else {
				/* wait for the decoder */
				dc.Signal();
				dc.WaitForDecoder();
				pc.Unlock();

				return true;
			}
		}
	}

	if (chunk == nullptr)
		chunk = pipe->Shift();

	assert(chunk != nullptr);

	/* insert the postponed tag if cross-fading is finished */

	if (xfade_state != CrossFadeState::ENABLED && cross_fade_tag != nullptr) {
		chunk->tag = Tag::MergeReplace(chunk->tag, cross_fade_tag);
		cross_fade_tag = nullptr;
	}

	/* play the current chunk */

	Error error;
	if (!play_chunk(pc, *song, chunk, buffer, play_audio_format, error)) {
		LogError(error);

		buffer.Return(chunk);

		pc.Lock();

		pc.SetError(PlayerError::OUTPUT, std::move(error));

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		pc.state = PlayerState::PAUSE;
		paused = true;

		pc.Unlock();

		idle_add(IDLE_PLAYER);

		return false;
	}

	/* this formula should prevent that the decoder gets woken up
	   with each chunk; it is more efficient to make it decode a
	   larger block at a time */
	pc.Lock();
	if (!dc.IsIdle() &&
	    dc.pipe->GetSize() <= (pc.buffered_before_play +
				   buffer.GetSize() * 3) / 4) {
		if (!decoder_woken) {
			decoder_woken = true;
			dc.Signal();
		}
	} else
		decoder_woken = false;
	pc.Unlock();

	return true;
}

inline bool
Player::SongBorder()
{
	xfade_state = CrossFadeState::UNKNOWN;

	FormatDefault(player_domain, "played \"%s\"", song->GetURI());

	ReplacePipe(dc.pipe);

	pc.outputs.SongBorder();

	if (!WaitForDecoder())
		return false;

	pc.Lock();

	const bool border_pause = pc.border_pause;
	if (border_pause) {
		paused = true;
		pc.state = PlayerState::PAUSE;
	}

	pc.Unlock();

	if (border_pause)
		idle_add(IDLE_PLAYER);

	return true;
}

inline void
Player::Run()
{
	pipe = new MusicPipe();

	StartDecoder(*pipe);
	if (!WaitForDecoder()) {
		assert(song == nullptr);

		StopDecoder();
		player_command_finished(pc);
		delete pipe;
		return;
	}

	pc.Lock();
	pc.state = PlayerState::PLAY;

	if (pc.command == PlayerCommand::SEEK)
		elapsed_time = pc.seek_time;

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

			if (!CheckDecoderStartup())
				break;

			pc.Lock();
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
			if (cross_fade_chunks > 0) {
				xfade_state = CrossFadeState::ENABLED;
				cross_fading = false;
			} else
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

			if (!SongBorder())
				break;
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
			if (!SendSilence())
				break;
		}

		pc.Lock();
	}

	StopDecoder();

	ClearAndDeletePipe();

	delete cross_fade_tag;

	if (song != nullptr) {
		FormatDefault(player_domain, "played \"%s\"", song->GetURI());
		delete song;
	}

	pc.Lock();

	pc.ClearTaggedSong();

	if (queued) {
		assert(pc.next_song != nullptr);
		delete pc.next_song;
		pc.next_song = nullptr;
	}

	pc.state = PlayerState::STOP;

	pc.Unlock();
}

static void
do_play(PlayerControl &pc, DecoderControl &dc,
	MusicBuffer &buffer)
{
	Player player(pc, dc, buffer);
	player.Run();
}

static void
player_task(void *arg)
{
	PlayerControl &pc = *(PlayerControl *)arg;

	SetThreadName("player");

	DecoderControl dc(pc.mutex, pc.cond);
	decoder_thread_start(dc);

	MusicBuffer buffer(pc.buffer_chunks);

	pc.Lock();

	while (1) {
		switch (pc.command) {
		case PlayerCommand::SEEK:
		case PlayerCommand::QUEUE:
			assert(pc.next_song != nullptr);

			pc.Unlock();
			do_play(pc, dc, buffer);
			pc.listener.OnPlayerSync();
			pc.Lock();
			break;

		case PlayerCommand::STOP:
			pc.Unlock();
			pc.outputs.Cancel();
			pc.Lock();

			/* fall through */

		case PlayerCommand::PAUSE:
			delete pc.next_song;
			pc.next_song = nullptr;

			pc.CommandFinished();
			break;

		case PlayerCommand::CLOSE_AUDIO:
			pc.Unlock();

			pc.outputs.Release();

			pc.Lock();
			pc.CommandFinished();

			assert(buffer.IsEmptyUnsafe());

			break;

		case PlayerCommand::UPDATE_AUDIO:
			pc.Unlock();
			pc.outputs.EnableDisable();
			pc.Lock();
			pc.CommandFinished();
			break;

		case PlayerCommand::EXIT:
			pc.Unlock();

			dc.Quit();

			pc.outputs.Close();

			player_command_finished(pc);
			return;

		case PlayerCommand::CANCEL:
			delete pc.next_song;
			pc.next_song = nullptr;

			pc.CommandFinished();
			break;

		case PlayerCommand::REFRESH:
			/* no-op when not playing */
			pc.CommandFinished();
			break;

		case PlayerCommand::NONE:
			pc.Wait();
			break;
		}
	}
}

void
StartPlayerThread(PlayerControl &pc)
{
	assert(!pc.thread.IsDefined());

	Error error;
	if (!pc.thread.Start(player_task, &pc, error))
		FatalError(error);
}
