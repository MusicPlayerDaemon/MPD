/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_DECODER_CONTROL_H
#define MPD_DECODER_CONTROL_H

#include "decoder_api.h"

#include "audio_format.h"
#include "notify.h"

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

	struct notify notify;

	volatile enum decoder_state state;
	volatile enum decoder_command command;
	bool quit;
	bool seek_error;
	bool seekable;
	volatile double seek_where;

	/** the format of the song file */
	struct audio_format in_audio_format;

	/** the format being sent to the music pipe */
	struct audio_format out_audio_format;

	struct song *current_song;
	struct song *next_song;
	float total_time;
};

extern struct decoder_control dc;

void dc_init(void);

void dc_deinit(void);

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
dc_command_wait(struct notify *notify);

void
dc_start(struct notify *notify, struct song *song);

void
dc_start_async(struct song *song);

void
dc_stop(struct notify *notify);

bool
dc_seek(struct notify *notify, double where);

void
dc_quit(void);

#endif
