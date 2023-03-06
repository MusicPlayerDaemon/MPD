// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
