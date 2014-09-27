/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "VorbisComment.hxx"
#include "util/ASCII.hxx"

#include <assert.h>
#include <string.h>

const char *
vorbis_comment_value(const char *entry, const char *name)
{
	assert(entry != nullptr);
	assert(name != nullptr);
	assert(*name != 0);

	const size_t length = strlen(name);

	if (StringEqualsCaseASCII(entry, name, length) &&
	    entry[length] == '=')
		return entry + length + 1;

	return nullptr;
}
