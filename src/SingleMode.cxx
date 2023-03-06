// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SingleMode.hxx"
#include "util/Compiler.h"

#include <cassert>
#include <stdexcept>

#include <string.h>

const char *
SingleToString(SingleMode mode) noexcept
{
	switch (mode) {
	case SingleMode::OFF:
		return "0";

	case SingleMode::ON:
		return "1";

	case SingleMode::ONE_SHOT:
		return "oneshot";
	}

	assert(false);
	gcc_unreachable();
}

SingleMode
SingleFromString(const char *s)
{
	assert(s != nullptr);

	if (strcmp(s, "0") == 0)
		return SingleMode::OFF;
	else if (strcmp(s, "1") == 0)
		return SingleMode::ON;
	else if (strcmp(s, "oneshot") == 0)
		return SingleMode::ONE_SHOT;
	else
		throw std::invalid_argument("Unrecognized single mode, expected 0, 1, or oneshot");
}
