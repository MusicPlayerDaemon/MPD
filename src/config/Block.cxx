/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "config.h"
#include "Block.hxx"
#include "ConfigParser.hxx"
#include "system/FatalError.hxx"

#include <stdlib.h>

int
BlockParam::GetIntValue() const
{
	char *endptr;
	long value2 = strtol(value.c_str(), &endptr, 0);
	if (*endptr != 0)
		FormatFatalError("Not a valid number in line %i", line);

	return value2;
}

unsigned
BlockParam::GetUnsignedValue() const
{
	char *endptr;
	unsigned long value2 = strtoul(value.c_str(), &endptr, 0);
	if (*endptr != 0)
		FormatFatalError("Not a valid number in line %i", line);

	return (unsigned)value2;
}

bool
BlockParam::GetBoolValue() const
{
	bool value2;
	if (!get_bool(value.c_str(), &value2))
		FormatFatalError("%s is not a boolean value (yes, true, 1) or "
				 "(no, false, 0) on line %i\n",
				 name.c_str(), line);

	return value2;
}
