// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Mixer.hxx"

#include <cassert>

void
Mixer::LockOpen()
{
	const std::scoped_lock lock{mutex};

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
	const std::scoped_lock lock{mutex};

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

int
Mixer::LockGetVolume()
{
	const std::scoped_lock lock{mutex};

	if (!open) {
		if (IsGlobal() && !failure)
			_Open();
		else
			return -1;
	}

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

	const std::scoped_lock lock{mutex};

	if (!open) {
		if (failure)
			std::rethrow_exception(failure);
		else if (IsGlobal())
			_Open();
		else
			return;
	}

	SetVolume(volume);
}
