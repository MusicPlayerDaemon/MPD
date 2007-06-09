/* the Music Player Daemon (MPD)
 * Copyright (C) 2007 by Warren Dukes (warren.dukes@gmail.com)
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

#ifndef MPD_TIMER_H
#define MPD_TIMER_H

#include "audio.h"
#include "mpd_types.h"

typedef struct _Timer {
	uint64_t start_time;
	uint64_t time;
	int started;
	int rate;
} Timer;

Timer *timer_new(AudioFormat *af);

void timer_free(Timer *timer);

void timer_start(Timer *timer);

void timer_reset(Timer *timer);

void timer_add(Timer *timer, int size);

void timer_sync(Timer *timer);

int timer_get_runtime_ms(Timer *timer);

#endif
