/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "ReplayGainMode.hxx"
#include "util/Compiler.h"

#include <cassert>
#include <stdexcept>

#include <string.h>

const char *
ToString(ReplayGainMode mode) noexcept
{
	switch (mode) {
	case ReplayGainMode::AUTO:
		return "auto";

	case ReplayGainMode::OFF:
		return "off";

	case ReplayGainMode::TRACK:
		return "track";

	case ReplayGainMode::ALBUM:
		return "album";
	}

	assert(false);
	gcc_unreachable();
}

ReplayGainMode
FromString(const char *s)
{
	assert(s != nullptr);

	if (strcmp(s, "off") == 0)
		return ReplayGainMode::OFF;
	else if (strcmp(s, "track") == 0)
		return ReplayGainMode::TRACK;
	else if (strcmp(s, "album") == 0)
		return ReplayGainMode::ALBUM;
	else if (strcmp(s, "auto") == 0)
		return ReplayGainMode::AUTO;
	else
		throw std::invalid_argument("Unrecognized replay gain mode");
}
