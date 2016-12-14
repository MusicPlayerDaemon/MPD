/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "AudioFormat.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "thread/Thread.hxx"
#include "CrossFade.hxx"
#include "Chrono.hxx"
#include "ReplayGainConfig.hxx"
#include "ReplayGainMode.hxx"

#include <exception>

#include <stdint.h>

class PlayerListener;
class MultipleOutputs;
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

struct player_status {
	PlayerState state;
	uint16_t bit_rate;
	AudioFormat audio_format;
	SignedSongTime total_time;
	SongTime elapsed_time;
};

struct PlayerControl final : AudioOutputClient {
	PlayerListener &listener;

	MultipleOutputs &outputs;

	const unsigned buffer_chunks;

	const unsigned buffered_before_play;

	/**
	 * The "audio_output_format" setting.
	 */
	const AudioFormat configured_audio_format;

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

	PlayerCommand command = PlayerCommand::NONE;
	PlayerState state = PlayerState::STOP;

	PlayerError error_type = PlayerError::NONE;

	/**
	 * The error that occurred in the player thread.  This
	 * attribute is only valid if #error_type is not
	 * #PlayerError::NONE.  The object must be freed when this
	 * object transitions back to #PlayerError::NONE.
	 */
	std::exception_ptr error;

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
	DetachedSong *tagged_song = nullptr;

	uint16_t bit_rate;
	AudioFormat audio_format;
	SignedSongTime total_time;
	SongTime elapsed_time;

	/**
	 * The next queued song.
	 *
	 * This is a duplicate, and must be freed when this attribute
	 * is cleared.
	 */
	DetachedSong *next_song = nullptr;

	SongTime seek_time;

	CrossFadeSettings cross_fade;

	const ReplayGainConfig replay_gain_config;
	ReplayGainMode replay_gain_mode = ReplayGainMode::OFF;

	double total_play_time = 0;

	/**
	 * If this flag is set, then the player will be auto-paused at
	 * the end of the song, before the next song starts to play.
	 *
	 * This is a copy of the queue's "single" flag most of the
	 * time.
	 */
	bool border_pause = false;

	PlayerControl(PlayerListener &_listener,
		      MultipleOutputs &_outputs,
		      unsigned buffer_chunks,
		      unsigned buffered_before_play,
		      AudioFormat _configured_audio_format,
		      const ReplayGainConfig &_replay_gain_config);
	~PlayerControl();

	/**
	 * Locks the object.
	 */
	void Lock() const {
		mutex.lock();
	}

	/**
	 * Unlocks the object.
	 */
	void Unlock() const {
		mutex.unlock();
	}

	/**
	 * Signals the object.  The object should be locked prior to
	 * calling this function.
	 */
	void Signal() {
		cond.signal();
	}

	/**
	 * Signals the object.  The object is temporarily locked by
	 * this function.
	 */
	void LockSignal() {
		const ScopeLock protect(mutex);
		Signal();
	}

	/**
	 * Waits for a signal on the object.  This function is only
	 * valid in the player thread.  The object must be locked
	 * prior to calling this function.
	 */
	void Wait() {
		assert(thread.IsInside());

		cond.wait(mutex);
	}

	/**
	 * Wake up the client waiting for command completion.
	 *
	 * Caller must lock the object.
	 */
	void ClientSignal() {
		assert(thread.IsInside());

		client_cond.signal();
	}

	/**
	 * The client calls this method to wait for command
	 * completion.
	 *
	 * Caller must lock the object.
	 */
	void ClientWait() {
		assert(!thread.IsInside());

		client_cond.wait(mutex);
	}

	/**
	 * A command has been finished.  This method clears the
	 * command and signals the client.
	 *
	 * To be called from the player thread.  Caller must lock the
	 * object.
	 */
	void CommandFinished() {
		assert(command != PlayerCommand::NONE);

		command = PlayerCommand::NONE;
		ClientSignal();
	}

	void LockCommandFinished() {
		const ScopeLock protect(mutex);
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
	bool WaitOutputConsumed(unsigned threshold);

	bool LockWaitOutputConsumed(unsigned threshold) {
		const ScopeLock protect(mutex);
		return WaitOutputConsumed(threshold);
	}

private:
	/**
	 * Wait for the command to be finished by the player thread.
	 *
	 * To be called from the main thread.  Caller must lock the
	 * object.
	 */
	void WaitCommandLocked() {
		while (command != PlayerCommand::NONE)
			ClientWait();
	}

	/**
	 * Send a command to the player thread and synchronously wait
	 * for it to finish.
	 *
	 * To be called from the main thread.  Caller must lock the
	 * object.
	 */
	void SynchronousCommand(PlayerCommand cmd) {
		assert(command == PlayerCommand::NONE);

		command = cmd;
		Signal();
		WaitCommandLocked();
	}

	/**
	 * Send a command to the player thread and synchronously wait
	 * for it to finish.
	 *
	 * To be called from the main thread.  This method locks the
	 * object.
	 */
	void LockSynchronousCommand(PlayerCommand cmd) {
		const ScopeLock protect(mutex);
		SynchronousCommand(cmd);
	}

public:
	/**
	 * Throws std::runtime_error or #Error on error.
	 *
	 * @param song the song to be queued; the given instance will
	 * be owned and freed by the player
	 */
	void Play(DetachedSong *song);

	/**
	 * see PlayerCommand::CANCEL
	 */
	void LockCancel();

	void LockSetPause(bool pause_flag);

private:
	void PauseLocked();

	void ClearError() {
		error_type = PlayerError::NONE;
		error = std::exception_ptr();
	}

public:
	void LockPause();

	/**
	 * Set the player's #border_pause flag.
	 */
	void LockSetBorderPause(bool border_pause);

	bool ApplyBorderPause() {
		if (border_pause)
			state = PlayerState::PAUSE;
		return border_pause;
	}

	bool LockApplyBorderPause() {
		const ScopeLock lock(mutex);
		return ApplyBorderPause();
	}

	void Kill();

	gcc_pure
	player_status LockGetStatus();

	PlayerState GetState() const {
		return state;
	}

	/**
	 * Set the error.  Discards any previous error condition.
	 *
	 * Caller must lock the object.
	 *
	 * @param type the error type; must not be #PlayerError::NONE
	 */
	void SetError(PlayerError type, std::exception_ptr &&_error);

	/**
	 * Set the error and set state to PlayerState::PAUSE.
	 */
	void SetOutputError(std::exception_ptr &&_error) {
		SetError(PlayerError::OUTPUT, std::move(_error));

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		state = PlayerState::PAUSE;
	}

	void LockSetOutputError(std::exception_ptr &&_error) {
		const ScopeLock lock(mutex);
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
	 * Like CheckRethrowError(), but locks and unlocks the object.
	 */
	void LockCheckRethrowError() const {
		const ScopeLock protect(mutex);
		CheckRethrowError();
	}

	void LockClearError();

	PlayerError GetErrorType() const {
		return error_type;
	}

	/**
	 * Set the #tagged_song attribute to a newly allocated copy of
	 * the given #DetachedSong.  Locks and unlocks the object.
	 */
	void LockSetTaggedSong(const DetachedSong &song);

	void ClearTaggedSong();

	/**
	 * Read and clear the #tagged_song attribute.
	 *
	 * Caller must lock the object.
	 */
	DetachedSong *ReadTaggedSong() {
		DetachedSong *result = tagged_song;
		tagged_song = nullptr;
		return result;
	}

	/**
	 * Like ReadTaggedSong(), but locks and unlocks the object.
	 */
	DetachedSong *LockReadTaggedSong() {
		const ScopeLock protect(mutex);
		return ReadTaggedSong();
	}

	void LockStop();

	void LockUpdateAudio();

private:
	void EnqueueSongLocked(DetachedSong *song) {
		assert(song != nullptr);
		assert(next_song == nullptr);

		next_song = song;
		seek_time = SongTime::zero();
		SynchronousCommand(PlayerCommand::QUEUE);
	}

	/**
	 * Throws std::runtime_error or #Error on error.
	 */
	void SeekLocked(DetachedSong *song, SongTime t);

public:
	/**
	 * @param song the song to be queued; the given instance will be owned
	 * and freed by the player
	 */
	void LockEnqueueSong(DetachedSong *song);

	/**
	 * Makes the player thread seek the specified song to a position.
	 *
	 * Throws std::runtime_error or #Error on error.
	 *
	 * @param song the song to be queued; the given instance will be owned
	 * and freed by the player
	 */
	void LockSeek(DetachedSong *song, SongTime t);

	void SetCrossFade(float cross_fade_seconds);

	float GetCrossFade() const {
		return cross_fade.duration;
	}

	void SetMixRampDb(float mixramp_db);

	float GetMixRampDb() const {
		return cross_fade.mixramp_db;
	}

	void SetMixRampDelay(float mixramp_delay_seconds);

	float GetMixRampDelay() const {
		return cross_fade.mixramp_delay;
	}

	void LockSetReplayGainMode(ReplayGainMode _mode) {
		const ScopeLock protect(mutex);
		replay_gain_mode = _mode;
	}

	double GetTotalPlayTime() const {
		return total_play_time;
	}

	/* virtual methods from AudioOutputClient */
	void ChunksConsumed() override {
		LockSignal();
	}

	void ApplyEnabled() override {
		LockUpdateAudio();
	}
};

#endif
