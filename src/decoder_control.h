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

	/**
	 * The song currently being decoded.  This attribute is set by
	 * the player thread, when it sends the #DECODE_COMMAND_START
	 * command.
	 */
	const struct song *song;

	float total_time;

	/** the #music_chunk allocator */
	struct music_buffer *buffer;

	/**
	 * The destination pipe for decoded chunks.  The caller thread
	 * owns this object, and is responsible for freeing it.
	 */
	struct music_pipe *pipe;
};

void
dc_init(struct decoder_control *dc);

void
dc_deinit(struct decoder_control *dc);

/**
 * Locks the #decoder_control object.
 */
static inline void
decoder_lock(struct decoder_control *dc)
{
	g_mutex_lock(dc->mutex);
}

/**
 * Unlocks the #decoder_control object.
 */
static inline void
decoder_unlock(struct decoder_control *dc)
{
	g_mutex_unlock(dc->mutex);
}

/**
 * Waits for a signal on the #decoder_control object.  This function
 * is only valid in the decoder thread.  The object must be locked
 * prior to calling this function.
 */
static inline void
decoder_wait(struct decoder_control *dc)
{
	g_cond_wait(dc->cond, dc->mutex);
}

/**
 * Signals the #decoder_control object.  This function is only valid
 * in the player thread.  The object should be locked prior to calling
 * this function.
 */
static inline void
decoder_signal(struct decoder_control *dc)
{
	g_cond_signal(dc->cond);
}

static inline bool
decoder_is_idle(const struct decoder_control *dc)
{
	return dc->state == DECODE_STATE_STOP ||
		dc->state == DECODE_STATE_ERROR;
}

static inline bool
decoder_is_starting(const struct decoder_control *dc)
{
	return dc->state == DECODE_STATE_START;
}

static inline bool
decoder_has_failed(const struct decoder_control *dc)
{
	assert(dc->command == DECODE_COMMAND_NONE);

	return dc->state == DECODE_STATE_ERROR;
}

static inline bool
decoder_lock_is_idle(struct decoder_control *dc)
{
	bool ret;

	decoder_lock(dc);
	ret = decoder_is_idle(dc);
	decoder_unlock(dc);

	return ret;
}

static inline bool
decoder_lock_is_starting(struct decoder_control *dc)
{
	bool ret;

	decoder_lock(dc);
	ret = decoder_is_starting(dc);
	decoder_unlock(dc);

	return ret;
}

static inline bool
decoder_lock_has_failed(struct decoder_control *dc)
{
	bool ret;

	decoder_lock(dc);
	ret = decoder_has_failed(dc);
	decoder_unlock(dc);

	return ret;
}

static inline const struct song *
decoder_current_song(const struct decoder_control *dc)
{
	switch (dc->state) {
	case DECODE_STATE_STOP:
	case DECODE_STATE_ERROR:
		return NULL;

	case DECODE_STATE_START:
	case DECODE_STATE_DECODE:
		return dc->song;
	}

	assert(false);
	return NULL;
}

void
dc_command_wait(struct decoder_control *dc);

/**
 * Start the decoder.
 *
 * @param the decoder
 * @param song the song to be decoded
 * @param pipe the pipe which receives the decoded chunks (owned by
 * the caller)
 */
void
dc_start(struct decoder_control *dc, struct song *song,
	 struct music_buffer *buffer, struct music_pipe *pipe);

void
dc_stop(struct decoder_control *dc);

bool
dc_seek(struct decoder_control *dc, double where);

void
dc_quit(struct decoder_control *dc);

#endif
