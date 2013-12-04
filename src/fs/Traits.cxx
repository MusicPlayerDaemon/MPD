/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "Traits.hxx"

#include <string.h>

PathTraitsFS::string
PathTraitsFS::Build(PathTraitsFS::const_pointer a, size_t a_size,
		    PathTraitsFS::const_pointer b, size_t b_size)
{
	assert(a != nullptr);
	assert(b != nullptr);

	if (a_size == 0)
		return string(b, b_size);
	if (b_size == 0)
		return string(a, a_size);

	string result(a, a_size);

	if (!IsSeparator(a[a_size - 1]))
		result.push_back(SEPARATOR);

	if (IsSeparator(b[0]))
		result.append(b + 1, b_size - 1);
	else
		result.append(b, b_size);

	return result;
}

PathTraitsUTF8::const_pointer
PathTraitsUTF8::GetBase(PathTraitsUTF8::const_pointer p)
{
	assert(p != nullptr);

	const char *slash = strrchr(p, SEPARATOR);
	return slash != nullptr
		? slash + 1
		: p;
}

PathTraitsUTF8::string
PathTraitsUTF8::GetParent(PathTraitsUTF8::const_pointer p)
{
	assert(p != nullptr);

	const char *slash = strrchr(p, SEPARATOR);
	return slash != nullptr
		? std::string(p, slash)
		: std::string(".");
}
