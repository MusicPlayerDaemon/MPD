/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "notify.h"
#include "audio_format.h"

#include <stdint.h>

enum player_state {
	PLAYER_STATE_STOP = 0,
	PLAYER_STATE_PAUSE,
	PLAYER_STATE_PLAY
};

enum player_command {
	PLAYER_COMMAND_NONE = 0,
	PLAYER_COMMAND_EXIT,
	PLAYER_COMMAND_STOP,
	PLAYER_COMMAND_PLAY,
	PLAYER_COMMAND_PAUSE,
	PLAYER_COMMAND_SEEK,
	PLAYER_COMMAND_CLOSE_AUDIO,

	/** player_control.next_song has been updated */
	PLAYER_COMMAND_QUEUE,

	/**
	 * cancel pre-decoding player_control.next_song; if the player
	 * has already started playing this song, it will completely
	 * stop
	 */
	PLAYER_COMMAND_CANCEL,
};

enum player_error {
	PLAYER_ERROR_NOERROR = 0,
	PLAYER_ERROR_FILE,
	PLAYER_ERROR_AUDIO,
	PLAYER_ERROR_SYSTEM,
	PLAYER_ERROR_UNKTYPE,
	PLAYER_ERROR_FILENOTFOUND,
};

struct player_status {
	enum player_state state;
	uint16_t bit_rate;
	struct audio_format audio_format;
	float total_time;
	float elapsed_time;
};

struct player_control {
	unsigned buffer_chunks;

	unsigned int buffered_before_play;

	/** the handle of the player thread, or NULL if the player
	    thread isn't running */
	GThread *thread;

	struct notify notify;
	volatile enum player_command command;
	volatile enum player_state state;
	volatile enum player_error error;
	uint16_t bit_rate;
	struct audio_format audio_format;
	float total_time;
	float elapsed_time;
	struct song *volatile next_song;
	struct song *errored_song;
	volatile double seek_where;
	float cross_fade_seconds;
	double total_play_time;
};

extern struct player_control pc;

void pc_init(unsigned buffer_chunks, unsigned buffered_before_play);

void pc_deinit(void);

/**
 * Call this function when the specified song pointer is about to be
 * invalidated.  This makes sure that player_control.errored_song does
 * not point to an invalid pointer.
 */
void
pc_song_deleted(const struct song *song);

void
pc_play(struct song *song);

/**
 * see PLAYER_COMMAND_CANCEL
 */
void pc_cancel(void);

void
pc_set_pause(bool pause_flag);

void
pc_pause(void);

void
pc_kill(void);

void
pc_get_status(struct player_status *status);

enum player_state
pc_get_state(void);

void
pc_clear_error(void);

/**
 * Returns the human-readable message describing the last error during
 * playback, NULL if no error occurred.  The caller has to free the
 * returned string.
 */
char *
pc_get_error_message(void);

enum player_error
pc_get_error(void);

void
pc_stop(void);

void
pc_enqueue_song(struct song *song);

/**
 * Makes the player thread seek the specified song to a position.
 *
 * @return true on success, false on failure (e.g. if MPD isn't
 * playing currently)
 */
bool
pc_seek(struct song *song, float seek_time);

void
pc_set_cross_fade(float cross_fade_seconds);

float
pc_get_cross_fade(void);

double
pc_get_total_play_time(void);

#endif
