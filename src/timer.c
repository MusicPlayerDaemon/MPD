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

#include "config.h"
#include "timer.h"
#include "audio_format.h"
#include "clock.h"

#include <glib.h>

#include <assert.h>
#include <limits.h>
#include <stddef.h>

struct timer *timer_new(const struct audio_format *af)
{
	struct timer *timer = g_new(struct timer, 1);
	timer->time = 0; // us
	timer->started = 0; // false
	timer->rate = af->sample_rate * audio_format_frame_size(af); // samples per second

	return timer;
}

void timer_free(struct timer *timer)
{
	g_free(timer);
}

void timer_start(struct timer *timer)
{
	timer->time = monotonic_clock_us();
	timer->started = 1;
}

void timer_reset(struct timer *timer)
{
	timer->time = 0;
	timer->started = 0;
}

void timer_add(struct timer *timer, int size)
{
	assert(timer->started);

	// (size samples) / (rate samples per second) = duration seconds
	// duration seconds * 1000000 = duration us
	timer->time += ((uint64_t)size * 1000000) / timer->rate;
}

unsigned
timer_delay(const struct timer *timer)
{
	int64_t delay = (int64_t)(timer->time - monotonic_clock_us()) / 1000;
	if (delay < 0)
		return 0;

	if (delay > G_MAXINT)
		delay = G_MAXINT;

	return delay;
}

void timer_sync(struct timer *timer)
{
	int64_t sleep_duration;

	assert(timer->started);

	sleep_duration = timer->time - monotonic_clock_us();
	if (sleep_duration > 0)
		g_usleep(sleep_duration);
}
