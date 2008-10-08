/* the Music Player Daemon (MPD)
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

#include "decoder_control.h"

struct decoder_control dc;

void dc_init(void)
{
	notify_init(&dc.notify);
	dc.state = DECODE_STATE_STOP;
	dc.command = DECODE_COMMAND_NONE;
	dc.error = DECODE_ERROR_NOERROR;
}

void dc_deinit(void)
{
	notify_deinit(&dc.notify);
}

void
dc_command_wait(struct notify *notify)
{
	while (dc.command != DECODE_COMMAND_NONE) {
		notify_signal(&dc.notify);
		notify_wait(notify);
	}
}

static void
dc_command(struct notify *notify, enum decoder_command cmd)
{
	dc.command = cmd;
	dc_command_wait(notify);
}

static void dc_command_async(enum decoder_command cmd)
{
	dc.command = cmd;
	notify_signal(&dc.notify);
}

void
dc_start(struct notify *notify, struct song *song)
{
	assert(song != NULL);

	dc.next_song = song;
	dc.error = DECODE_ERROR_NOERROR;
	dc_command(notify, DECODE_COMMAND_START);
}

void
dc_start_async(struct song *song)
{
	assert(song != NULL);

	dc.next_song = song;
	dc.error = DECODE_ERROR_NOERROR;
	dc_command_async(DECODE_COMMAND_START);
}

void
dc_stop(struct notify *notify)
{
	if (dc.command == DECODE_COMMAND_START ||
	    dc.state != DECODE_STATE_STOP)
		dc_command(notify, DECODE_COMMAND_STOP);
}

int
dc_seek(struct notify *notify, double where)
{
	assert(where >= 0.0);

	if (dc.state == DECODE_STATE_STOP || !dc.seekable)
		return -1;

	dc.seekWhere = where;
	dc.seekError = 0;
	dc_command(notify, DECODE_COMMAND_SEEK);

	if (dc.seekError)
		return -1;

	return 0;
}
