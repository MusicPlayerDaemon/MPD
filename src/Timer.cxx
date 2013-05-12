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
#include "Timer.hxx"
#include "audio_format.h"
#include "clock.h"

#include <glib.h>

#include <assert.h>
#include <limits.h>
#include <stddef.h>

Timer::Timer(const struct audio_format &af)
	: time(0),
	  started(false),
	  rate(af.sample_rate * audio_format_frame_size(&af))
{
}

void Timer::Start()
{
	time = monotonic_clock_us();
	started = true;
}

void Timer::Reset()
{
	time = 0;
	started = false;
}

void Timer::Add(int size)
{
	assert(started);

	// (size samples) / (rate samples per second) = duration seconds
	// duration seconds * 1000000 = duration us
	time += ((uint64_t)size * 1000000) / rate;
}

unsigned Timer::GetDelay() const
{
	int64_t delay = (int64_t)(time - monotonic_clock_us()) / 1000;
	if (delay < 0)
		return 0;

	if (delay > G_MAXINT)
		delay = G_MAXINT;

	return delay;
}

void Timer::Synchronize() const
{
	int64_t sleep_duration;

	assert(started);

	sleep_duration = time - monotonic_clock_us();
	if (sleep_duration > 0)
		g_usleep(sleep_duration);
}
