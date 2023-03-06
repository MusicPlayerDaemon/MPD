// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Timer.hxx"
#include "pcm/AudioFormat.hxx"

#include <cassert>

Timer::Timer(const AudioFormat af) noexcept
	:rate(af.sample_rate * af.GetFrameSize())
{
}

void
Timer::Start() noexcept
{
	time = Now();
	started = true;
}

void
Timer::Reset() noexcept
{
	started = false;
}

void
Timer::Add(size_t size) noexcept
{
	assert(started);

	// (size samples) / (rate samples per second) = duration seconds
	// duration seconds * 1000000 = duration us
	time += Time(((uint64_t)size * Time::period::den) / (Time::period::num * rate));
}

std::chrono::steady_clock::duration
Timer::GetDelay() const noexcept
{
	assert(started);

	const auto delay = time - Now();
	if (delay < Time::zero())
		return Time::zero();

	return std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay);
}
