/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_PLAYER_CONTROL_HXX
#define MPD_PLAYER_CONTROL_HXX

#include "output/Client.hxx"
#include "config/PlayerConfig.hxx"
#include "pcm/AudioFormat.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "thread/Thread.hxx"
#include "CrossFade.hxx"
#include "Chrono.hxx"
#include "ReplayGainMode.hxx"
#include "MusicChunkPtr.hxx"

#include <cstdint>
#include <exception>
#include <memory>

struct Tag;
struct PlayerConfig;
class PlayerListener;
class PlayerOutputs;
class InputCacheManager;
class DetachedSong;

enum class PlayerState : uint8_t {
	STOP,
	PAUSE,
	PLAY
};

enum class PlayerCommand : uint8_t {
	NONE,
	EXIT,
	STOP,
	PAUSE,

	/**
	 * Seek to a certain position in the specified song.  This
	 * command can also be used to change the current song or
	 * start playback.  It "finishes" immediately, but
	 * PlayerControl::seeking will be set until seeking really
	 * completes (or fails).
	 */
	SEEK,

	CLOSE_AUDIO,

	/**
	 * At least one AudioOutput.enabled flag has been modified;
	 * commit those changes to the output threads.
	 */
	UPDATE_AUDIO,

	/** PlayerControl.next_song has been updated */
	QUEUE,

	/**
	 * cancel pre-decoding PlayerControl.next_song; if the player
	 * has already started playing this song, it will completely
	 * stop
	 */
	CANCEL,

	/**
	 * Refresh status information in the #PlayerControl struct,
	 * e.g. elapsed_time.
	 */
	REFRESH,
};

enum class PlayerError : uint8_t {
	NONE,

	/**
	 * The decoder has failed to decode the song.
	 */
	DECODER,

	/**
	 * The audio output has failed.
	 */
	OUTPUT,
};

struct PlayerStatus {
	PlayerState state;
	uint16_t bit_rate;
	AudioFormat audio_format;
	SignedSongTime total_time;
	SongTime elapsed_time;
};

class PlayerControl final : public AudioOutputClient {
	friend class Player;

	PlayerListener &listener;

	PlayerOutputs &outputs;

	InputCacheManager *const input_cache;

	const PlayerConfig config;

	/**
	 * The handle of the player thread.
	 */
	Thread thread;

	/**
	 * This lock protects #command, #state, #error, #tagged_song.
	 */
	mutable Mutex mutex;

	/**
	 * Trigger this object after you have modified #command.
	 */
	Cond cond;

	/**
	 * This object gets signalled when the player thread has
	 * finished the #command.  It wakes up the client that waits
	 * (i.e. the main thread).
	 */
	Cond client_cond;

	/**
	 * The error that occurred in the player thread.  This
	 * attribute is only valid if #error_type is not
	 * #PlayerError::NONE.  The object must be freed when this
	 * object transitions back to #PlayerError::NONE.
	 */
	std::exception_ptr error;

	/**
	 * The next queued song.
	 *
	 * This is a duplicate, and must be freed when this attribute
	 * is cleared.
	 */
	std::unique_ptr<DetachedSong> next_song;

	/**
	 * A copy of the current #DetachedSong after its tags have
	 * been updated by the decoder (for example, a radio stream
	 * that has sent a new tag after switching to the next song).
	 * This shall be used by PlayerListener::OnPlayerTagModified()
	 * to update the current #DetachedSong in the queue.
	 *
	 * Protected by #mutex.  Set by the PlayerThread and consumed
	 * by the main thread.
	 */
	std::unique_ptr<DetachedSong> tagged_song;

	PlayerCommand command = PlayerCommand::NONE;
	PlayerState state = PlayerState::STOP;

	PlayerError error_type = PlayerError::NONE;

	ReplayGainMode replay_gain_mode = ReplayGainMode::OFF;

	/**
	 * Is the player currently busy with the SEEK command?
	 */
	bool seeking = false;

	/**
	 * If this flag is set, then the player will be auto-paused at
	 * the end of the song, before the next song starts to play.
	 *
	 * This is a copy of the queue's "single" flag most of the
	 * time.
	 */
	bool border_pause = false;

	/**
	 * If this flag is set, then the player thread is currently
	 * occupied and will not be able to respond quickly to
	 * commands (e.g. waiting for the decoder thread to finish
	 * seeking).  This is used to skip #PlayerCommand::REFRESH to
	 * avoid blocking the main thread.
	 */
	bool occupied = false;

	struct ScopeOccupied {
		PlayerControl &pc;

		explicit ScopeOccupied(PlayerControl &_pc) noexcept:pc(_pc) {
			assert(!pc.occupied);
			pc.occupied = true;
		}

		~ScopeOccupied() noexcept {
			assert(pc.occupied);
			pc.occupied = false;
		}
	};

	AudioFormat audio_format;
	uint16_t bit_rate;

	SignedSongTime total_time;
	SongTime elapsed_time;

	SongTime seek_time;

	CrossFadeSettings cross_fade;

	FloatDuration total_play_time = FloatDuration::zero();

public:
	PlayerControl(PlayerListener &_listener,
		      PlayerOutputs &_outputs,
		      InputCacheManager *_input_cache,
		      const PlayerConfig &_config) noexcept;
	~PlayerControl() noexcept;

	void Kill() noexcept;

	/**
	 * Like CheckRethrowError(), but locks and unlocks the object.
	 */
	void LockCheckRethrowError() const {
		const std::scoped_lock<Mutex> protect(mutex);
		CheckRethrowError();
	}

	void LockClearError() noexcept;

	PlayerError GetErrorType() const noexcept {
		return error_type;
	}

	void LockUpdateAudio() noexcept;

	/**
	 * Throws on error.
	 *
	 * @param song the song to be queued
	 */
	void Play(std::unique_ptr<DetachedSong> song);

	/**
	 * @param song the song to be queued; the given instance will be owned
	 * and freed by the player
	 */
	void LockEnqueueSong(std::unique_ptr<DetachedSong> song) noexcept;

	/**
	 * Makes the player thread seek the specified song to a position.
	 *
	 * Throws on error.
	 *
	 * @param song the song to be queued; the given instance will be owned
	 * and freed by the player
	 */
	void LockSeek(std::unique_ptr<DetachedSong> song, SongTime t);

	void LockStop() noexcept;

	/**
	 * see PlayerCommand::CANCEL
	 */
	void LockCancel() noexcept;

	void LockSetPause(bool pause_flag) noexcept;

	void LockPause() noexcept;

	/**
	 * Set the player's #border_pause flag.
	 */
	void LockSetBorderPause(bool border_pause) noexcept;
	void SetCrossFade(FloatDuration duration) noexcept;

	auto GetCrossFade() const noexcept {
		return cross_fade.duration;
	}

	void SetMixRampDb(float mixramp_db) noexcept;

	float GetMixRampDb() const noexcept {
		return cross_fade.mixramp_db;
	}

	void SetMixRampDelay(FloatDuration mixramp_delay) noexcept;

	auto GetMixRampDelay() const noexcept {
		return cross_fade.mixramp_delay;
	}

	void LockSetReplayGainMode(ReplayGainMode _mode) noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		replay_gain_mode = _mode;
	}

	/**
	 * Like ReadTaggedSong(), but locks and unlocks the object.
	 */
	std::unique_ptr<DetachedSong> LockReadTaggedSong() noexcept;

	[[gnu::pure]]
	PlayerStatus LockGetStatus() noexcept;

	PlayerState GetState() const noexcept {
		return state;
	}

	struct SyncInfo {
		PlayerState state;
		bool has_next_song;
	};

	[[gnu::pure]]
	SyncInfo LockGetSyncInfo() const noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		return {state, next_song != nullptr};
	}

	auto GetTotalPlayTime() const noexcept {
		return total_play_time;
	}

private:
	/**
	 * Signals the object.  The object should be locked prior to
	 * calling this function.
	 */
	void Signal() noexcept {
		cond.notify_one();
	}

	/**
	 * Signals the object.  The object is temporarily locked by
	 * this function.
	 */
	void LockSignal() noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		Signal();
	}

	/**
	 * Waits for a signal on the object.  This function is only
	 * valid in the player thread.  The object must be locked
	 * prior to calling this function.
	 */
	void Wait(std::unique_lock<Mutex> &lock) noexcept {
		assert(thread.IsInside());

		cond.wait(lock);
	}

	/**
	 * Wake up the client waiting for command completion.
	 *
	 * Caller must lock the object.
	 */
	void ClientSignal() noexcept {
		assert(thread.IsInside());

		client_cond.notify_one();
	}

	/**
	 * The client calls this method to wait for command
	 * completion.
	 *
	 * Caller must lock the object.
	 */
	void ClientWait(std::unique_lock<Mutex> &lock) noexcept {
		assert(!thread.IsInside());

		client_cond.wait(lock);
	}

	/**
	 * A command has been finished.  This method clears the
	 * command and signals the client.
	 *
	 * To be called from the player thread.  Caller must lock the
	 * object.
	 */
	void CommandFinished() noexcept {
		assert(command != PlayerCommand::NONE);

		command = PlayerCommand::NONE;
		ClientSignal();
	}

	void LockCommandFinished() noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		CommandFinished();
	}

	/**
	 * Checks if the size of the #MusicPipe is below the #threshold.  If
	 * not, it attempts to synchronize with all output threads, and waits
	 * until another #MusicChunk is finished.
	 *
	 * Caller must lock the mutex.
	 *
	 * @param threshold the maximum number of chunks in the pipe
	 * @return true if there are less than #threshold chunks in the pipe
	 */
	bool WaitOutputConsumed(std::unique_lock<Mutex> &lock,
				unsigned threshold) noexcept;

	bool LockWaitOutputConsumed(unsigned threshold) noexcept {
		std::unique_lock<Mutex> lock(mutex);
		return WaitOutputConsumed(lock, threshold);
	}

	/**
	 * Wait for the command to be finished by the player thread.
	 *
	 * To be called from the main thread.  Caller must lock the
	 * object.
	 */
	void WaitCommandLocked(std::unique_lock<Mutex> &lock) noexcept {
		while (command != PlayerCommand::NONE)
			ClientWait(lock);
	}

	/**
	 * Send a command to the player thread and synchronously wait
	 * for it to finish.
	 *
	 * To be called from the main thread.  Caller must lock the
	 * object.
	 */
	void SynchronousCommand(std::unique_lock<Mutex> &lock,
				PlayerCommand cmd) noexcept {
		assert(command == PlayerCommand::NONE);

		command = cmd;
		Signal();
		WaitCommandLocked(lock);
	}

	/**
	 * Send a command to the player thread and synchronously wait
	 * for it to finish.
	 *
	 * To be called from the main thread.  This method locks the
	 * object.
	 */
	void LockSynchronousCommand(PlayerCommand cmd) noexcept {
		std::unique_lock<Mutex> lock(mutex);
		SynchronousCommand(lock, cmd);
	}

	void PauseLocked(std::unique_lock<Mutex> &lock) noexcept;

	void ClearError() noexcept {
		error_type = PlayerError::NONE;
		error = std::exception_ptr();
	}

	bool ApplyBorderPause() noexcept {
		if (border_pause)
			state = PlayerState::PAUSE;
		return border_pause;
	}

	/**
	 * Set the error.  Discards any previous error condition.
	 *
	 * Caller must lock the object.
	 *
	 * @param type the error type; must not be #PlayerError::NONE
	 */
	void SetError(PlayerError type, std::exception_ptr &&_error) noexcept;

	/**
	 * Set the error and set state to PlayerState::PAUSE.
	 */
	void SetOutputError(std::exception_ptr &&_error) noexcept {
		SetError(PlayerError::OUTPUT, std::move(_error));

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		state = PlayerState::PAUSE;
	}

	void LockSetOutputError(std::exception_ptr &&_error) noexcept {
		const std::scoped_lock<Mutex> lock(mutex);
		SetOutputError(std::move(_error));
	}

	/**
	 * Checks whether an error has occurred, and if so, rethrows
	 * it.
	 *
	 * Caller must lock the object.
	 */
	void CheckRethrowError() const {
		if (error_type != PlayerError::NONE)
			std::rethrow_exception(error);
	}

	/**
	 * Set the #tagged_song attribute to a newly allocated copy of
	 * the given #DetachedSong.  Locks and unlocks the object.
	 */
	void LockSetTaggedSong(const DetachedSong &song) noexcept;

	void ClearTaggedSong() noexcept;

	/**
	 * Read and clear the #tagged_song attribute.
	 *
	 * Caller must lock the object.
	 */
	std::unique_ptr<DetachedSong> ReadTaggedSong() noexcept;

	void EnqueueSongLocked(std::unique_lock<Mutex> &lock,
			       std::unique_ptr<DetachedSong> song) noexcept;

	/**
	 * Throws on error.
	 */
	void SeekLocked(std::unique_lock<Mutex> &lock,
			std::unique_ptr<DetachedSong> song, SongTime t);

	/**
	 * Caller must lock the object.
	 */
	void CancelPendingSeek() noexcept {
		if (!seeking)
			return;

		seeking = false;
		ClientSignal();
	}

	void LockUpdateSongTag(DetachedSong &song,
			       const Tag &new_tag) noexcept;

	/**
	 * Plays a #MusicChunk object (after applying software
	 * volume).  If it contains a (stream) tag, copy it to the
	 * current song, so MPD's playlist reflects the new stream
	 * tag.
	 *
	 * Player lock is not held.
	 *
	 * Throws on error.
	 */
	void PlayChunk(DetachedSong &song, MusicChunkPtr chunk,
		       const AudioFormat &format);

	/* virtual methods from AudioOutputClient */
	void ChunksConsumed() override {
		LockSignal();
	}

	void ApplyEnabled() override {
		LockUpdateAudio();
	}

	void RunThread() noexcept;
};

#endif
