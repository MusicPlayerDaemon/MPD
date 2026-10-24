// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Mixer.hxx"

#include <algorithm> // for std::clamp()
#include <cassert>
#include <cmath> // for std::lround()

void
Mixer::LockOpen()
{
	const std::lock_guard lock{mutex};

	if (open)
		return;

	_Open();
}

void
Mixer::_Open()
{
	assert(!open);

	try {
		Open();
		open = true;
		failure = {};
	} catch (...) {
		failure = std::current_exception();
		throw;
	}
}

void
Mixer::LockClose() noexcept
{
	const std::lock_guard lock{mutex};

	if (open)
		_Close();
}

void
Mixer::_Close() noexcept
{
	assert(open);

	Close();
	open = false;
	failure = {};
}

bool
Mixer::EnsureOpen(bool rethrow)
{
	if (open)
		return true;
	else if (rethrow && failure)
		std::rethrow_exception(failure);
	else if (IsGlobal()) {
		_Open();
		return true;
	} else
		return false;
}

int
Mixer::LockGetVolume()
{
	const std::lock_guard lock{mutex};

	if (!EnsureOpen(false))
		return -1;

	try {
		return GetVolume();
	} catch (...) {
		_Close();
		failure = std::current_exception();
		throw;
	}
}

void
Mixer::LockSetVolume(unsigned volume)
{
	assert(volume <= 100);

	const std::lock_guard lock{mutex};

	if (!EnsureOpen(true))
		return;

	SetVolume(volume);
}

int
Mixer::LockSetScaledVolume(unsigned base, float scale)
{
	assert(base <= 100);
	assert(scale >= 0);

	const std::lock_guard lock{mutex};

	if (!EnsureOpen(true))
		return -1;

	const unsigned volume = ScaleVolume(base, scale);
	assert(volume <= 100);

	SetVolume(volume);
	return volume;
}

unsigned
Mixer::ScaleVolume(unsigned volume, float scale) noexcept
{
	return std::clamp(std::lround(volume * scale), 0L, 100L);
}
