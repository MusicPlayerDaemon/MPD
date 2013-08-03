/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_PLAYER_H
#define MPD_PLAYER_H

#include "AudioFormat.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"

#include <glib.h>

#include <stdint.h>

struct decoder_control;
struct Song;

enum player_state {
	PLAYER_STATE_STOP = 0,
	PLAYER_STATE_PAUSE,
	PLAYER_STATE_PLAY
};

enum player_command {
	PLAYER_COMMAND_NONE = 0,
	PLAYER_COMMAND_EXIT,
	PLAYER_COMMAND_STOP,
	PLAYER_COMMAND_PAUSE,
	PLAYER_COMMAND_SEEK,
	PLAYER_COMMAND_CLOSE_AUDIO,

	/**
	 * At least one audio_output.enabled flag has been modified;
	 * commit those changes to the output threads.
	 */
	PLAYER_COMMAND_UPDATE_AUDIO,

	/** player_control.next_song has been updated */
	PLAYER_COMMAND_QUEUE,

	/**
	 * cancel pre-decoding player_control.next_song; if the player
	 * has already started playing this song, it will completely
	 * stop
	 */
	PLAYER_COMMAND_CANCEL,

	/**
	 * Refresh status information in the #player_control struct,
	 * e.g. elapsed_time.
	 */
	PLAYER_COMMAND_REFRESH,
};

enum player_error {
	PLAYER_ERROR_NONE = 0,

	/**
	 * The decoder has failed to decode the song.
	 */
	PLAYER_ERROR_DECODER,

	/**
	 * The audio output has failed.
	 */
	PLAYER_ERROR_OUTPUT,
};

struct player_status {
	enum player_state state;
	uint16_t bit_rate;
	AudioFormat audio_format;
	float total_time;
	float elapsed_time;
};

struct player_control {
	unsigned buffer_chunks;

	unsigned int buffered_before_play;

	/** the handle of the player thread, or NULL if the player
	    thread isn't running */
	GThread *thread;

	/**
	 * This lock protects #command, #state, #error.
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

	enum player_command command;
	enum player_state state;

	enum player_error error_type;

	/**
	 * The error that occurred in the player thread.  This
	 * attribute is only valid if #error is not
	 * #PLAYER_ERROR_NONE.  The object must be freed when this
	 * object transitions back to #PLAYER_ERROR_NONE.
	 */
	GError *error;

	uint16_t bit_rate;
	AudioFormat audio_format;
	float total_time;
	float elapsed_time;

	/**
	 * The next queued song.
	 *
	 * This is a duplicate, and must be freed when this attribute
	 * is cleared.
	 */
	Song *next_song;

	double seek_where;
	float cross_fade_seconds;
	float mixramp_db;
	float mixramp_delay_seconds;
	double total_play_time;

	/**
	 * If this flag is set, then the player will be auto-paused at
	 * the end of the song, before the next song starts to play.
	 *
	 * This is a copy of the queue's "single" flag most of the
	 * time.
	 */
	bool border_pause;

	player_control(unsigned buffer_chunks,
		       unsigned buffered_before_play);
	~player_control();

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
		assert(thread == g_thread_self());

		cond.wait(mutex);
	}

	/**
	 * Wake up the client waiting for command completion.
	 *
	 * Caller must lock the object.
	 */
	void ClientSignal() {
		assert(thread == g_thread_self());

		client_cond.signal();
	}

	/**
	 * The client calls this method to wait for command
	 * completion.
	 *
	 * Caller must lock the object.
	 */
	void ClientWait() {
		assert(thread != g_thread_self());

		client_cond.wait(mutex);
	}

	/**
	 * @param song the song to be queued; the given instance will
	 * be owned and freed by the player
	 */
	void Play(Song *song);

	/**
	 * see PLAYER_COMMAND_CANCEL
	 */
	void Cancel();

	void SetPause(bool pause_flag);

	void Pause();

	/**
	 * Set the player's #border_pause flag.
	 */
	void SetBorderPause(bool border_pause);

	void Kill();

	gcc_pure
	player_status GetStatus();

	player_state GetState() const {
		return state;
	}

	/**
	 * Set the error.  Discards any previous error condition.
	 *
	 * Caller must lock the object.
	 *
	 * @param type the error type; must not be #PLAYER_ERROR_NONE
	 * @param error detailed error information; must not be NULL; the
	 * #player_control takes over ownership of this #GError instance
	 */
	void SetError(player_error type, GError *error);

	void ClearError();

	/**
	 * Returns the human-readable message describing the last
	 * error during playback, NULL if no error occurred.  The
	 * caller has to free the returned string.
	 */
	char *GetErrorMessage() const;

	player_error GetErrorType() const {
		return error_type;
	}

	void Stop();

	void UpdateAudio();

	/**
	 * @param song the song to be queued; the given instance will be owned
	 * and freed by the player
	 */
	void EnqueueSong(Song *song);

	/**
	 * Makes the player thread seek the specified song to a position.
	 *
	 * @param song the song to be queued; the given instance will be owned
	 * and freed by the player
	 * @return true on success, false on failure (e.g. if MPD isn't
	 * playing currently)
	 */
	bool Seek(Song *song, float seek_time);

	void SetCrossFade(float cross_fade_seconds);

	float GetCrossFade() const {
		return cross_fade_seconds;
	}

	void SetMixRampDb(float mixramp_db);

	float GetMixRampDb() const {
		return mixramp_db;
	}

	void SetMixRampDelay(float mixramp_delay_seconds);

	float GetMixRampDelay() const {
		return mixramp_delay_seconds;
	}

	double GetTotalPlayTime() const {
		return total_play_time;
	}
};

#endif
