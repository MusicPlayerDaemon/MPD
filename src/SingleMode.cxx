/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "SingleMode.hxx"

#include <stdexcept>

#include <assert.h>
#include <string.h>

const char *
SingleToString(SingleMode mode) noexcept
{
	switch (mode) {
	case SingleMode::ONE_SHOT:
		return "oneshot";

	case SingleMode::OFF:
		return "off";

	case SingleMode::ON:
		return "on";
	}

	assert(false);
	gcc_unreachable();
}

SingleMode
SingleFromString(const char *s)
{
	assert(s != nullptr);

	if (strcmp(s, "off") == 0)
		return SingleMode::OFF;
	else if (strcmp(s, "on") == 0)
		return SingleMode::ON;
	else if (strcmp(s, "oneshot") == 0)
		return SingleMode::ONE_SHOT;
	// backward compatibility?
	else if (strcmp(s, "1") == 0)
		return SingleMode::ON;
	else if (strcmp(s, "0") == 0)
		return SingleMode::OFF;
	else
		throw std::invalid_argument("Unrecognized single mode");
}
