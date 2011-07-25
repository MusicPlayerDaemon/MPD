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

#ifndef MPD_TIMER_H
#define MPD_TIMER_H

#include <stdint.h>

struct audio_format;

struct timer {
	uint64_t time;
	int started;
	int rate;
};

struct timer *timer_new(const struct audio_format *af);

void timer_free(struct timer *timer);

void timer_start(struct timer *timer);

void timer_reset(struct timer *timer);

void timer_add(struct timer *timer, int size);

/**
 * Returns the number of milliseconds to sleep to get back to sync.
 */
unsigned
timer_delay(const struct timer *timer);

void timer_sync(struct timer *timer);

#endif
