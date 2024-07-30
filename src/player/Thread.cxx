// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/* \file
 *
 * The player thread controls the playback.  It acts as a bridge
 * between the decoder thread and the output thread(s): it receives
 * #MusicChunk objects from the decoder, optionally mixes them
 * (cross-fading), applies software volume, and sends them to the
 * audio outputs via PlayerOutputs::Play()
 * (i.e. MultipleOutputs::Play()).
 *
 * It is controlled by the main thread (the playlist code), see
 * Control.hxx.  The playlist enqueues new songs into the player
 * thread and sends it commands.
 *
 * The player thread itself does not do any I/O.  It synchronizes with
 * other threads via #GMutex and #GCond objects, and passes
 * #MusicChunk instances around in #MusicPipe objects.
 */

#include "Control.hxx"
#include "Outputs.hxx"
#include "Listener.hxx"
#include "decoder/Control.hxx"
#include "MusicPipe.hxx"
#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"
#include "song/DetachedSong.hxx"
#include "CrossFade.hxx"
#include "pcm/MixRampGlue.hxx"
#include "tag/Tag.hxx"
#include "util/Domain.hxx"
#include "thread/Name.hxx"
#include "Log.hxx"

#include <exception>
#include <memory>

#include <string.h>

static constexpr Domain player_domain("player");

/**
 * Start playback as soon as enough data for this duration has been
 * pushed to the decoder pipe.
 */
static constexpr auto buffer_before_play_duration = std::chrono::seconds(1);

class Player {
	PlayerControl &pc;

	DecoderControl &dc;

	MusicBuffer &buffer;

	std::shared_ptr<MusicPipe> pipe;

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
	 * Start playback as soon as this number of chunks has been
	 * pushed to the decoder pipe.  This is calculated based on
	 * #buffer_before_play_duration.
	 */
	unsigned buffer_before_play;

	/**
	 * If the decoder pipe gets consumed below this threshold,
	 * it's time to wake up the decoder.
	 *
	 * It is calculated in a way which should prevent a wakeup
	 * after each single consumed chunk; it is more efficient to
	 * make the decoder decode a larger block at a time.
	 */
	const unsigned decoder_wakeup_threshold;

	/**
	 * Are we waiting for #buffer_before_play?
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

	/**
	 * If this is positive, then we need to ask the decoder to
	 * seek after it has completed startup.  This is needed if the
	 * decoder is in the middle of startup while the player
	 * receives another seek command.
	 *
	 * This is only valid while #decoder_starting is true.
	 */
	SongTime pending_seek;

public:
	Player(PlayerControl &_pc, DecoderControl &_dc,
	       MusicBuffer &_buffer) noexcept
		:pc(_pc), dc(_dc), buffer(_buffer),
		 decoder_wakeup_threshold(buffer.GetSize() * 3 / 4)
	{
	}

private:
	/**
	 * Reset cross-fading to the initial state.  A check to
	 * re-enable it at an appropriate time will be scheduled.
	 */
	void ResetCrossFade() noexcept {
		xfade_state = CrossFadeState::UNKNOWN;
	}

	template<typename P>
	void ReplacePipe(P &&_pipe) noexcept {
		ResetCrossFade();
		pipe = std::forward<P>(_pipe);
	}

	/**
	 * Start the decoder.
	 *
	 * Caller must lock the mutex.
	 */
	void StartDecoder(std::unique_lock<Mutex> &lock,
			  std::shared_ptr<MusicPipe> pipe,
			  bool initial_seek_essential) noexcept;

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
	bool CheckDecoderStartup(std::unique_lock<Mutex> &lock) noexcept;

	/**
	 * Stop the decoder and clears (and frees) its music pipe.
	 *
	 * Caller must lock the mutex.
	 */
	void StopDecoder(std::unique_lock<Mutex> &lock) noexcept;

	/**
	 * Is the decoder still busy on the same song as the player?
	 *
	 * Note: this function does not check if the decoder is already
	 * finished.
	 */
	[[nodiscard]] [[gnu::pure]]
	bool IsDecoderAtCurrentSong() const noexcept {
		assert(pipe != nullptr);

		return dc.pipe == pipe;
	}

	/**
	 * Returns true if the decoder is decoding the next song (or has begun
	 * decoding it, or has finished doing it), and the player hasn't
	 * switched to that song yet.
	 */
	[[nodiscard]] [[gnu::pure]]
	bool IsDecoderAtNextSong() const noexcept {
		return dc.pipe != nullptr && !IsDecoderAtCurrentSong();
	}

	/**
	 * Invoke DecoderControl::Seek() and update our state or
	 * handle errors.
	 *
	 * Caller must lock the mutex.
	 *
	 * @return false if the decoder has failed
	 */
	bool SeekDecoder(std::unique_lock<Mutex> &lock,
			 SongTime seek_time) noexcept;

	/**
	 * This is the handler for the #PlayerCommand::SEEK command.
	 *
	 * Caller must lock the mutex.
	 *
	 * @return false if the decoder has failed
	 */
	bool SeekDecoder(std::unique_lock<Mutex> &lock) noexcept;

	void CancelPendingSeek() noexcept {
		pending_seek = SongTime::zero();
		pc.CancelPendingSeek();
	}

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
	 * Caller must lock the mutex.
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

	std::string UnlockAnalyzeMixRamp(const MusicPipe &pipe,
					 const AudioFormat &audio_format,
					 MixRampDirection direction) noexcept;

	/**
	 * @return false if more chunks of the next song are needed to
	 * scan for MixRamp data
	 */
	[[nodiscard]]
	bool MixRampScannerReady() noexcept;

	void CheckCrossFade() noexcept;

	/**
	 * Obtains the next chunk from the music pipe, optionally applies
	 * cross-fading, and sends it to all audio outputs.
	 *
	 * @return true on success, false on error (playback will be stopped)
	 */
	bool PlayNextChunk() noexcept;

	unsigned UnlockCheckOutputs() noexcept {
		const ScopeUnlock unlock(pc.mutex);
		return pc.outputs.CheckPipe();
	}

	/**
	 * Player lock must be held before calling.
	 *
	 * @return false to stop playback
	 */
	bool ProcessCommand(std::unique_lock<Mutex> &lock) noexcept;

	/**
	 * This is called at the border between two songs: the audio output
	 * has consumed all chunks of the current song, and we should start
	 * sending chunks from the next one.
	 *
	 * Caller must lock the mutex.
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
Player::StartDecoder(std::unique_lock<Mutex> &lock,
		     std::shared_ptr<MusicPipe> _pipe,
		     bool initial_seek_essential) noexcept
{
	assert(!decoder_starting);
	assert(queued || pc.command == PlayerCommand::SEEK);
	assert(pc.next_song != nullptr);

	/* copy ReplayGain parameters to the decoder */
	dc.replay_gain_mode = pc.replay_gain_mode;

	SongTime start_time = pc.next_song->GetStartTime() + pc.seek_time;

	dc.Start(lock, std::make_unique<DetachedSong>(*pc.next_song),
		 start_time, pc.next_song->GetEndTime(),
		 initial_seek_essential,
		 buffer, std::move(_pipe));
}

void
Player::StopDecoder(std::unique_lock<Mutex> &lock) noexcept
{
	const PlayerControl::ScopeOccupied occupied(pc);

	dc.Stop(lock);

	if (dc.pipe != nullptr) {
		/* clear and free the decoder pipe */

		dc.pipe->Clear();
		dc.pipe.reset();

		/* just in case we've been cross-fading: cancel it
		   now, because we just deleted the new song's decoder
		   pipe */
		ResetCrossFade();
	}

	decoder_starting = false;
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

	pc.ClearTaggedSong();

	song = std::exchange(pc.next_song, nullptr);

	elapsed_time = pc.seek_time;

	/* set the "starting" flag, which will be cleared by
	   CheckDecoderStartup() */
	decoder_starting = true;
	pending_seek = SongTime::zero();

	/* update PlayerControl's song information */
	pc.total_time = song->GetDuration();
	pc.bit_rate = 0;
	pc.audio_format.Clear();

	{
		/* call playlist::SyncWithPlayer() in the main thread */
		const ScopeUnlock unlock(pc.mutex);
		pc.listener.OnPlayerSync();
	}
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
		return {end_time - start_time};

	return {SongTime(decoder_duration) - start_time};
}

std::string
Player::UnlockAnalyzeMixRamp(const MusicPipe &_pipe,
			    const AudioFormat &audio_format,
			    MixRampDirection direction) noexcept
{
	const ScopeUnlock unlock(pc.mutex);
	return AnalyzeMixRamp(_pipe, audio_format, direction);
}

inline bool
Player::MixRampScannerReady() noexcept
{
	assert(pipe);
	assert(dc.pipe);

	if (!pc.cross_fade.IsMixRampEnabled())
		return true;

	if (!pc.config.mixramp_analyzer)
		/* always ready if the scanner is disabled */
		return true;

	if (dc.GetMixRampPreviousEnd() == nullptr) {
		// TODO: scan incrementally backwards until mixrampdb is reached
		auto s = UnlockAnalyzeMixRamp(*pipe, play_audio_format,
					      MixRampDirection::END);
		if (!s.empty()) {
			FmtDebug(player_domain, "Analyzed MixRamp end: {}", s);
			dc.SetMixRampPreviousEnd(std::move(s));
		}

		if (dc.GetMixRampStart() == nullptr)
			/* scan the next song in the next call; first,
			   let the main loop submit a few more chunks
			   to the outputs for playback to avoid
			   xrun */
			return false;
	}

	if (dc.GetMixRampStart() == nullptr) {
		const std::size_t want_pipe_bytes =
			dc.out_audio_format.TimeToSize(std::chrono::seconds{20});
		const std::size_t want_pipe_chunks =
			std::min((want_pipe_bytes + sizeof(MusicChunk::data) - 1)
				 / sizeof(MusicChunk::data),
				 buffer.GetSize() / std::size_t{3});

		if (dc.pipe->GetSize() < want_pipe_chunks) {
			/* need more data */
			if (!buffer.IsFull()) {
				decoder_woken = true;
				dc.Signal();
			}

			return false;
		}

		// TODO: scan incrementally until mixrampdb is reached
		auto s = UnlockAnalyzeMixRamp(*dc.pipe, dc.out_audio_format,
					      MixRampDirection::START);
		if (!s.empty()) {
			FmtDebug(player_domain, "Analyzed MixRamp start: {}", s);
			dc.SetMixRampStart(std::move(s));
		}
	}

	return true;
}

bool
Player::OpenOutput() noexcept
{
	assert(play_audio_format.IsDefined());
	assert(pc.state == PlayerState::PLAY ||
	       pc.state == PlayerState::PAUSE);

	try {
		const ScopeUnlock unlock(pc.mutex);
		pc.outputs.Open(play_audio_format);
	} catch (...) {
		LogError(std::current_exception());

		output_open = false;

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		paused = true;

		pc.SetOutputError(std::current_exception());

		return false;
	}

	output_open = true;
	paused = false;

	pc.state = PlayerState::PLAY;
	pc.listener.OnPlayerStateChanged();

	return true;
}

inline bool
Player::CheckDecoderStartup(std::unique_lock<Mutex> &lock) noexcept
{
	assert(decoder_starting);

	if (!ForwardDecoderError()) {
		/* the decoder failed */
		return false;
	} else if (!dc.IsStarting()) {
		/* the decoder is ready and ok */

		if (output_open &&
		    !pc.WaitOutputConsumed(lock, 1))
			/* the output devices havn't finished playing
			   all chunks yet - wait for that */
			return true;

		pc.total_time = real_song_duration(*dc.song,
						   dc.total_time);
		pc.audio_format = dc.in_audio_format;
		play_audio_format = dc.out_audio_format;
		decoder_starting = false;

		const size_t buffer_before_play_size =
			play_audio_format.TimeToSize(buffer_before_play_duration);
		buffer_before_play =
			(buffer_before_play_size + sizeof(MusicChunk::data) - 1)
			/ sizeof(MusicChunk::data);

		pc.listener.OnPlayerStateChanged();

		if (pending_seek > SongTime::zero()) {
			assert(pc.seeking);

			bool success = SeekDecoder(lock, pending_seek);
			pc.seeking = false;
			pc.ClientSignal();
			if (!success)
				return false;

			/* re-fill the buffer after seeking */
			buffering = true;
		} else if (pc.seeking) {
			pc.seeking = false;
			pc.ClientSignal();

			/* re-fill the buffer after seeking */
			buffering = true;
		}

		if (!paused && !OpenOutput()) {
			FmtError(player_domain,
				 "problems opening audio device "
				 "while playing {:?}",
				 dc.song->GetURI());
			return true;
		}

		return true;
	} else {
		/* the decoder is not yet ready; wait
		   some more */
		dc.WaitForDecoder(lock);

		return true;
	}
}

bool
Player::SeekDecoder(std::unique_lock<Mutex> &lock, SongTime seek_time) noexcept
{
	assert(song);
	assert(!decoder_starting);

	if (!pc.total_time.IsNegative()) {
		const SongTime total_time(pc.total_time);
		if (seek_time > total_time)
			seek_time = total_time;
	}

	try {
		const PlayerControl::ScopeOccupied occupied(pc);

		dc.Seek(lock, song->GetStartTime() + seek_time);
	} catch (...) {
		/* decoder failure */
		pc.SetError(PlayerError::DECODER, std::current_exception());
		return false;
	}

	elapsed_time = seek_time;
	return true;
}

inline bool
Player::SeekDecoder(std::unique_lock<Mutex> &lock) noexcept
{
	assert(pc.next_song != nullptr);

	if (pc.seek_time > SongTime::zero() && // TODO: allow this only if the song duration is known
	    dc.IsUnseekableCurrentSong(*pc.next_song)) {
		/* seeking into the current song; but we already know
		   it's not seekable, so let's fail early */
		/* note the seek_time>0 check: if seeking to the
		   beginning, we can simply restart the decoder */
		pc.next_song.reset();
		pc.SetError(PlayerError::DECODER,
			    std::make_exception_ptr(std::runtime_error("Not seekable")));
		pc.CommandFinished();
		return true;
	}

	CancelPendingSeek();

	{
		const ScopeUnlock unlock(pc.mutex);
		pc.outputs.Cancel();
	}

	pc.listener.OnPlayerStateChanged();

	if (!dc.IsSeekableCurrentSong(*pc.next_song)) {
		/* the decoder is already decoding the "next" song -
		   stop it and start the previous song again */

		StopDecoder(lock);

		/* clear music chunks which might still reside in the
		   pipe */
		pipe->Clear();

		/* re-start the decoder */
		StartDecoder(lock, pipe, true);
		ActivateDecoder();

		pc.seeking = true;
		pc.CommandFinished();

		assert(xfade_state == CrossFadeState::UNKNOWN);

		return true;
	} else {
		if (!IsDecoderAtCurrentSong()) {
			/* the decoder is already decoding the "next" song,
			   but it is the same song file; exchange the pipe */
			ReplacePipe(dc.pipe);
		}

		pc.next_song.reset();
		queued = false;

		if (decoder_starting) {
			/* wait for the decoder to complete
			   initialization; postpone the SEEK
			   command */

			pending_seek = pc.seek_time;
			pc.seeking = true;
			pc.CommandFinished();
			return true;
		} else {
			/* send the SEEK command */

			if (!SeekDecoder(lock, pc.seek_time)) {
				pc.CommandFinished();
				return false;
			}
		}
	}

	pc.CommandFinished();

	assert(xfade_state == CrossFadeState::UNKNOWN);

	/* re-fill the buffer after seeking */
	buffering = true;

	{
		/* call syncPlaylistWithQueue() in the main thread */
		const ScopeUnlock unlock(pc.mutex);
		pc.listener.OnPlayerSync();
	}

	return true;
}

inline bool
Player::ProcessCommand(std::unique_lock<Mutex> &lock) noexcept
{
	switch (pc.command) {
	case PlayerCommand::NONE:
		break;

	case PlayerCommand::STOP:
	case PlayerCommand::EXIT:
	case PlayerCommand::CLOSE_AUDIO:
		return false;

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
		return SeekDecoder(lock);

	case PlayerCommand::CANCEL:
		if (pc.next_song == nullptr)
			/* the cancel request arrived too late, we're
			   already playing the queued song...  stop
			   everything now */
			return false;

		if (IsDecoderAtNextSong())
			/* the decoder is already decoding the song -
			   stop it and reset the position */
			StopDecoder(lock);

		pc.next_song.reset();
		queued = false;
		pc.CommandFinished();
		break;

	case PlayerCommand::REFRESH:
		if (output_open && !paused) {
			const ScopeUnlock unlock(pc.mutex);
			pc.outputs.CheckPipe();
		}

		pc.elapsed_time = !pc.outputs.GetElapsedTime().IsNegative()
			? SongTime(pc.outputs.GetElapsedTime())
			: elapsed_time;

		pc.CommandFinished();
		break;
	}

	return true;
}

inline void
Player::CheckCrossFade() noexcept
{
	if (xfade_state != CrossFadeState::UNKNOWN)
		/* already decided */
		return;

	if (pc.border_pause) {
		/* no cross-fading if MPD is going to pause at the end
		   of the current song */
		xfade_state = CrossFadeState::UNKNOWN;
		return;
	}

	if (!IsDecoderAtNextSong() || dc.IsStarting() || dc.pipe->IsEmpty())
		/* we need information about the next song before we
		   can decide */
		/* the "pipe.empty" check is here so we wait for all
                   (ReplayGain/MixRamp) metadata to appear, which some
                   decoders parse only after reporting readiness */
		return;

	if (!pc.cross_fade.CanCrossFade(pc.total_time, dc.total_time,
					dc.out_audio_format,
					play_audio_format)) {
		/* cross fading is disabled or the next song is too
		   short */
		xfade_state = CrossFadeState::DISABLED;
		return;
	}

	if (!MixRampScannerReady())
		/* need more chunks for the MixRamp scanner */
		return;

	/* enable cross fading in this song?  if yes, calculate how
	   many chunks will be required for it */
	cross_fade_chunks =
		pc.cross_fade.Calculate(dc.replay_gain_db,
					dc.replay_gain_prev_db,
					dc.GetMixRampStart(),
					dc.GetMixRampPreviousEnd(),
					play_audio_format,
					buffer.GetSize() -
					buffer_before_play);
	if (cross_fade_chunks > 0)
		xfade_state = CrossFadeState::ENABLED;
	else
		// TODO: eliminate this "else" branch
		xfade_state = CrossFadeState::DISABLED;
}

inline void
PlayerControl::LockUpdateSongTag(DetachedSong &song,
				 const Tag &new_tag) noexcept
{
	if (song.IsFile())
		/* don't update tags of local files, only remote
		   streams may change tags dynamically */
		return;

	song.MergeTag(new_tag);

	LockSetTaggedSong(song);

	/* the main thread will update the playlist version when he
	   receives this event */
	listener.OnPlayerTagModified();
}

inline void
PlayerControl::PlayChunk(DetachedSong &song, MusicChunkPtr chunk,
			 const AudioFormat &format)
{
	assert(chunk->CheckFormat(format));

	if (chunk->tag != nullptr)
		LockUpdateSongTag(song, *chunk->tag);

	if (chunk->IsEmpty())
		return;

	{
		const std::scoped_lock lock{mutex};
		bit_rate = chunk->bit_rate;
	}

	/* send the chunk to the audio outputs */

	const double chunk_length(chunk->length);

	outputs.Play(std::move(chunk));
	total_play_time += format.SizeToTime<decltype(total_play_time)>(chunk_length);
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

	MusicChunkPtr chunk;
	if (xfade_state == CrossFadeState::ACTIVE) {
		/* perform cross fade */

		assert(IsDecoderAtNextSong());

		unsigned cross_fade_position = pipe->GetSize();
		assert(cross_fade_position <= cross_fade_chunks);

		auto other_chunk = dc.pipe->Shift();
		if (other_chunk != nullptr) {
			chunk = pipe->Shift();
			assert(chunk != nullptr);
			assert(chunk->other == nullptr);

			/* don't send the tags of the new song (which
			   is being faded in) yet; postpone it until
			   the current song is faded out */
			cross_fade_tag = Tag::Merge(std::move(cross_fade_tag),
						    std::move(other_chunk->tag));

			if (pc.cross_fade.mixramp_delay <= FloatDuration::zero()) {
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
				other_chunk.reset();
			}

			chunk->other = std::move(other_chunk);
		} else {
			/* there are not enough decoded chunks yet */

			std::unique_lock lock{pc.mutex};

			if (dc.IsIdle()) {
				/* the decoder isn't running, abort
				   cross fading */
				xfade_state = CrossFadeState::DISABLED;
			} else {
				/* wait for the decoder */
				dc.Signal();
				dc.WaitForDecoder(lock);

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
		pc.PlayChunk(*song, std::move(chunk),
			     play_audio_format);
	} catch (...) {
		LogError(std::current_exception());

		chunk.reset();

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		paused = true;

		pc.LockSetOutputError(std::current_exception());

		return false;
	}

	const std::scoped_lock lock{pc.mutex};

	/* this formula should prevent that the decoder gets woken up
	   with each chunk; it is more efficient to make it decode a
	   larger block at a time */
	if (!dc.IsIdle() && dc.pipe->GetSize() <= decoder_wakeup_threshold) {
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
	{
		const ScopeUnlock unlock(pc.mutex);

		FmtNotice(player_domain, "played {:?}", song->GetURI());

		ReplacePipe(dc.pipe);

		pc.outputs.SongBorder();
	}

	ActivateDecoder();

	const bool border_pause = pc.ApplyBorderPause();
	if (border_pause) {
		paused = true;
		pc.listener.OnBorderPause();

		/* drain all outputs to guarantee the current song is
		   really being played to the end; without this, the
		   Pause() call would drop all ring buffers */
		pc.outputs.Drain();

		pc.outputs.Pause();
		pc.listener.OnPlayerStateChanged();
	}
}

inline void
Player::Run() noexcept
{
	pipe = std::make_shared<MusicPipe>();

	std::unique_lock lock{pc.mutex};

	StartDecoder(lock, pipe, true);
	ActivateDecoder();

	pc.state = PlayerState::PLAY;

	pc.CommandFinished();

	while (ProcessCommand(lock)) {
		if (decoder_starting) {
			/* wait until the decoder is initialized completely */

			if (!CheckDecoderStartup(lock))
				break;

			continue;
		}

		if (buffering) {
			/* buffering at the start of the song - wait
			   until the buffer is large enough, to
			   prevent stuttering on slow machines */

			if (pipe->GetSize() < buffer_before_play &&
			    !dc.IsIdle() && !buffer.IsFull()) {
				/* not enough decoded buffer space yet */

				dc.WaitForDecoder(lock);
				continue;
			} else {
				/* buffering is complete */
				buffering = false;
			}
		}

		if (dc.IsIdle() && queued && IsDecoderAtCurrentSong()) {
			/* the decoder has finished the current song;
			   make it decode the next song */

			assert(dc.pipe == nullptr || dc.pipe == pipe);

			StartDecoder(lock, std::make_shared<MusicPipe>(),
				     false);
		}

		CheckCrossFade();

		if (paused) {
			if (pc.command == PlayerCommand::NONE)
				pc.Wait(lock);
		} else if (!pipe->IsEmpty()) {
			/* at least one music chunk is ready - send it
			   to the audio output */

			const ScopeUnlock unlock(pc.mutex);
			PlayNextChunk();
		} else if (UnlockCheckOutputs() > 0) {
			/* not enough data from decoder, but the
			   output thread is still busy, so it's
			   okay */

			/* wake up the decoder (just in case it's
			   waiting for space in the MusicBuffer) and
			   wait for it */
			// TODO: eliminate this kludge
			dc.Signal();

			dc.WaitForDecoder(lock);
		} else if (IsDecoderAtNextSong()) {
			/* at the beginning of a new song */

			SongBorder();
		} else if (dc.IsIdle()) {
			if (queued)
				/* the decoder has just stopped,
				   between the two IsIdle() checks,
				   probably while UnlockCheckOutputs()
				   left the mutex unlocked; to restart
				   the decoder instead of stopping
				   playback completely, let's re-enter
				   this loop */
				continue;

			/* check the size of the pipe again, because
			   the decoder thread may have added something
			   since we last checked */
			if (pipe->IsEmpty()) {
				/* wait for the hardware to finish
				   playback */
				const ScopeUnlock unlock(pc.mutex);
				pc.outputs.Drain();
				break;
			}
		} else if (output_open) {
			/* the decoder is too busy and hasn't provided
			   new PCM data in time: wait for the
			   decoder */

			/* wake up the decoder (just in case it's
			   waiting for space in the MusicBuffer) and
			   wait for it */
			// TODO: eliminate this kludge
			dc.Signal();

			dc.WaitForDecoder(lock);
		}
	}

	CancelPendingSeek();
	StopDecoder(lock);

	pipe.reset();

	cross_fade_tag.reset();

	if (song != nullptr) {
		FmtNotice(player_domain, "played {:?}", song->GetURI());
		song.reset();
	}

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
try {
	SetThreadName("player");

	DecoderControl dc(mutex, cond,
			  input_cache,
			  config.audio_format,
			  config.replay_gain);
	dc.StartThread();

	MusicBuffer buffer{config.buffer_chunks};

	std::unique_lock lock{mutex};

	while (true) {
		switch (command) {
		case PlayerCommand::SEEK:
		case PlayerCommand::QUEUE:
			assert(next_song != nullptr);

			{
				const ScopeUnlock unlock(mutex);
				do_play(*this, dc, buffer);

				/* give the main thread a chance to
				   queue another song, just in case
				   we've stopped playback
				   spuriously */
				listener.OnPlayerSync();
			}

			break;

		case PlayerCommand::STOP:
			{
				const ScopeUnlock unlock(mutex);
				outputs.Cancel();
			}

			/* fall through */
			[[fallthrough]];

		case PlayerCommand::PAUSE:
			next_song.reset();

			CommandFinished();
			break;

		case PlayerCommand::CLOSE_AUDIO:
			{
				const ScopeUnlock unlock(mutex);
				outputs.Release();
			}

			CommandFinished();

			assert(buffer.IsEmptyUnsafe());

			break;

		case PlayerCommand::UPDATE_AUDIO:
			{
				const ScopeUnlock unlock(mutex);
				outputs.EnableDisable();
			}

			CommandFinished();
			break;

		case PlayerCommand::EXIT:
			{
				const ScopeUnlock unlock(mutex);
				dc.Quit();
				outputs.Close();
			}

			CommandFinished();
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
			Wait(lock);
			break;
		}
	}
} catch (...) {
	/* exceptions caught here are thrown during initialization;
	   the main loop doesn't throw */

	LogError(std::current_exception());

	/* TODO: what now? How will the main thread learn about this
	   failure? */
}
