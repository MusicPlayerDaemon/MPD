// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ConsumeMode.hxx"
#include "util/Compiler.h"

#include <cassert>
#include <stdexcept>

#include <string.h>

const char *
ConsumeToString(ConsumeMode mode) noexcept
{
	switch (mode) {
	case ConsumeMode::OFF:
		return "0";

	case ConsumeMode::ON:
		return "1";

	case ConsumeMode::ONE_SHOT:
		return "oneshot";
	}

	assert(false);
	gcc_unreachable();
}

ConsumeMode
ConsumeFromString(const char *s)
{
	assert(s != nullptr);

	if (strcmp(s, "0") == 0)
		return ConsumeMode::OFF;
	else if (strcmp(s, "1") == 0)
		return ConsumeMode::ON;
	else if (strcmp(s, "oneshot") == 0)
		return ConsumeMode::ONE_SHOT;
	else
		throw std::invalid_argument("Unrecognized consume mode, expected 0, 1, or oneshot");
}
