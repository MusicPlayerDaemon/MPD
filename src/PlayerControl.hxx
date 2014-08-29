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

#ifndef MPD_PLAYER_CONTROL_HXX
#define MPD_PLAYER_CONTROL_HXX

#include "AudioFormat.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "thread/Thread.hxx"
#include "util/Error.hxx"
#include "CrossFade.hxx"
#include "Chrono.hxx"

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

struct PlayerControl {
	PlayerListener &listener;

	MultipleOutputs &outputs;

	const unsigned buffer_chunks;

	const unsigned buffered_before_play;

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

	PlayerCommand command;
	PlayerState state;

	PlayerError error_type;

	/**
	 * The error that occurred in the player thread.  This
	 * attribute is only valid if #error is not
	 * #PlayerError::NONE.  The object must be freed when this
	 * object transitions back to #PlayerError::NONE.
	 */
	Error error;

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
	DetachedSong *tagged_song;

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
	DetachedSong *next_song;

	SongTime seek_time;

	CrossFadeSettings cross_fade;

	double total_play_time;

	/**
	 * If this flag is set, then the player will be auto-paused at
	 * the end of the song, before the next song starts to play.
	 *
	 * This is a copy of the queue's "single" flag most of the
	 * time.
	 */
	bool border_pause;

	PlayerControl(PlayerListener &_listener,
		      MultipleOutputs &_outputs,
		      unsigned buffer_chunks,
		      unsigned buffered_before_play);
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
		Lock();
		Signal();
		Unlock();
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
		Lock();
		SynchronousCommand(cmd);
		Unlock();
	}

public:
	/**
	 * @param song the song to be queued; the given instance will
	 * be owned and freed by the player
	 */
	void Play(DetachedSong *song);

	/**
	 * see PlayerCommand::CANCEL
	 */
	void Cancel();

	void SetPause(bool pause_flag);

private:
	void PauseLocked();

public:
	void Pause();

	/**
	 * Set the player's #border_pause flag.
	 */
	void SetBorderPause(bool border_pause);

	void Kill();

	gcc_pure
	player_status GetStatus();

	PlayerState GetState() const {
		return state;
	}

	/**
	 * Set the error.  Discards any previous error condition.
	 *
	 * Caller must lock the object.
	 *
	 * @param type the error type; must not be #PlayerError::NONE
	 * @param error detailed error information; must be defined.
	 */
	void SetError(PlayerError type, Error &&error);

	/**
	 * Checks whether an error has occurred, and if so, returns a
	 * copy of the #Error object.
	 *
	 * Caller must lock the object.
	 */
	gcc_pure
	Error GetError() const {
		Error result;
		if (error_type != PlayerError::NONE)
			result.Set(error);
		return result;
	}

	/**
	 * Like GetError(), but locks and unlocks the object.
	 */
	gcc_pure
	Error LockGetError() const {
		Lock();
		Error result = GetError();
		Unlock();
		return result;
	}

	void ClearError();

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
		Lock();
		DetachedSong *result = ReadTaggedSong();
		Unlock();
		return result;
	}

	void Stop();

	void UpdateAudio();

private:
	void EnqueueSongLocked(DetachedSong *song) {
		assert(song != nullptr);
		assert(next_song == nullptr);

		next_song = song;
		SynchronousCommand(PlayerCommand::QUEUE);
	}

public:
	/**
	 * @param song the song to be queued; the given instance will be owned
	 * and freed by the player
	 */
	void EnqueueSong(DetachedSong *song);

	/**
	 * Makes the player thread seek the specified song to a position.
	 *
	 * @param song the song to be queued; the given instance will be owned
	 * and freed by the player
	 * @return true on success, false on failure (e.g. if MPD isn't
	 * playing currently)
	 */
	bool Seek(DetachedSong *song, SongTime t);

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

	double GetTotalPlayTime() const {
		return total_play_time;
	}
};

#endif
