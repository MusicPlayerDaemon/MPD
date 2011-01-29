/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "string_util.h"

#include <glib.h>

#include <assert.h>

const char *
strchug_fast_c(const char *p)
{
	while (*p != 0 && g_ascii_isspace(*p))
		++p;

	return p;
}

bool
string_array_contains(const char *const* haystack, const char *needle)
{
	assert(haystack != NULL);
	assert(needle != NULL);

	for (; *haystack != NULL; ++haystack)
		if (g_ascii_strcasecmp(*haystack, needle) == 0)
			return true;

	return false;
}
