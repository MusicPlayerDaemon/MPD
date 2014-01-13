/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "AudioFormat.hxx"
#include "system/Clock.hxx"

#include <limits>

#include <assert.h>

Timer::Timer(const AudioFormat af)
	: time(0),
	  started(false),
	 rate(af.sample_rate * af.GetFrameSize())
{
}

void Timer::Start()
{
	time = MonotonicClockUS();
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
	int64_t delay = (int64_t)(time - MonotonicClockUS()) / 1000;
	if (delay < 0)
		return 0;

	if (delay > std::numeric_limits<int>::max())
		delay = std::numeric_limits<int>::max();

	return delay;
}
