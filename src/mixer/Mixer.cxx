/*
 * Copyright 2003-2022 The Music Player Daemon Project
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
