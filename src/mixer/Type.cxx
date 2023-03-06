// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Type.hxx"

#include <cassert>
#include <stdexcept>

#include <string.h>

MixerType
mixer_type_parse(const char *input)
{
	assert(input != nullptr);

	if (strcmp(input, "none") == 0 || strcmp(input, "disabled") == 0)
		return MixerType::NONE;
	else if (strcmp(input, "hardware") == 0)
		return MixerType::HARDWARE;
	else if (strcmp(input, "software") == 0)
		return MixerType::SOFTWARE;
	else if (strcmp(input, "null") == 0)
		return MixerType::NULL_;
	else
		throw std::runtime_error("Unrecognized mixer type");
}
