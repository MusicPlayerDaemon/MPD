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

#ifndef MPD_DECODER_CONTROL_H
#define MPD_DECODER_CONTROL_H

#include "decoder_command.h"
#include "audio_format.h"

#include <glib.h>

#include <assert.h>

#define DECODE_TYPE_FILE	0
#define DECODE_TYPE_URL		1

enum decoder_state {
	DECODE_STATE_STOP = 0,
	DECODE_STATE_START,
	DECODE_STATE_DECODE,

	/**
	 * The last "START" command failed, because there was an I/O
	 * error or because no decoder was able to decode the file.
	 * This state will only come after START; once the state has
	 * turned to DECODE, by definition no such error can occur.
	 */
	DECODE_STATE_ERROR,
};

struct decoder_control {
	/** the handle of the decoder thread, or NULL if the decoder
	    thread isn't running */
	GThread *thread;

	/**
	 * This lock protects #state and #command.
	 */
	GMutex *mutex;

	/**
	 * Trigger this object after you have modified #command.  This
	 * is also used by the decoder thread to notify the caller
	 * when it has finished a command.
	 */
	GCond *cond;

	enum decoder_state state;
	enum decoder_command command;

	bool quit;
	bool seek_error;
	bool seekable;
	double seek_where;

	/** the format of the song file */
	struct audio_format in_audio_format;

	/** the format being sent to the music pipe */
	struct audio_format out_audio_format;

	struct song *current_song;
	struct song *next_song;
	float total_time;

	/** the #music_chunk allocator */
	struct music_buffer *buffer;

	/** the destination pipe for decoded chunks */
	struct music_pipe *pipe;
};

extern struct decoder_control dc;

void dc_init(void);

void dc_deinit(void);

/**
 * Locks the #decoder_control object.
 */
static inline void
decoder_lock(void)
{
	g_mutex_lock(dc.mutex);
}

/**
 * Unlocks the #decoder_control object.
 */
static inline void
decoder_unlock(void)
{
	g_mutex_unlock(dc.mutex);
}

/**
 * Waits for a signal on the #decoder_control object.  This function
 * is only valid in the decoder thread.  The object must be locked
 * prior to calling this function.
 */
static inline void
decoder_wait(void)
{
	g_cond_wait(dc.cond, dc.mutex);
}

/**
 * Signals the #decoder_control object.  This function is only valid
 * in the player thread.  The object should be locked prior to calling
 * this function.
 */
static inline void
decoder_signal(void)
{
	g_cond_signal(dc.cond);
}

static inline bool decoder_is_idle(void)
{
	return (dc.state == DECODE_STATE_STOP ||
		dc.state == DECODE_STATE_ERROR) &&
		dc.command != DECODE_COMMAND_START;
}

static inline bool decoder_is_starting(void)
{
	return dc.command == DECODE_COMMAND_START ||
		dc.state == DECODE_STATE_START;
}

static inline bool decoder_has_failed(void)
{
	assert(dc.command == DECODE_COMMAND_NONE);

	return dc.state == DECODE_STATE_ERROR;
}

static inline bool decoder_lock_is_idle(void)
{
	bool ret;

	decoder_lock();
	ret = decoder_is_idle();
	decoder_unlock();

	return ret;
}

static inline bool decoder_lock_is_starting(void)
{
	bool ret;

	decoder_lock();
	ret = decoder_is_starting();
	decoder_unlock();

	return ret;
}

static inline bool decoder_lock_has_failed(void)
{
	bool ret;

	decoder_lock();
	ret = decoder_has_failed();
	decoder_unlock();

	return ret;
}

static inline struct song *
decoder_current_song(void)
{
	switch (dc.state) {
	case DECODE_STATE_STOP:
	case DECODE_STATE_ERROR:
		return NULL;

	case DECODE_STATE_START:
	case DECODE_STATE_DECODE:
		return dc.current_song;
	}

	assert(false);
	return NULL;
}

void
dc_command_wait(void);

void
dc_start(struct song *song);

void
dc_start_async(struct song *song);

void
dc_stop(void);

bool
dc_seek(double where);

void
dc_quit(void);

#endif
