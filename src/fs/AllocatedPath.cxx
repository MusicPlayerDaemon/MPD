/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "AllocatedPath.hxx"
#include "Domain.hxx"
#include "Charset.hxx"
#include "util/Compiler.h"
#include "config.h"

#include <exception>

/* no inlining, please */
AllocatedPath::~AllocatedPath() {}

AllocatedPath
AllocatedPath::FromUTF8(const char *path_utf8) noexcept
{
#if defined(HAVE_FS_CHARSET) || defined(_WIN32)
	try {
		return AllocatedPath(::PathFromUTF8(path_utf8));
	} catch (...) {
		return nullptr;
	}
#else
	return FromFS(path_utf8);
#endif
}

AllocatedPath
AllocatedPath::FromUTF8Throw(const char *path_utf8)
{
#if defined(HAVE_FS_CHARSET) || defined(_WIN32)
	return AllocatedPath(::PathFromUTF8(path_utf8));
#else
	return FromFS(path_utf8);
#endif
}

void
AllocatedPath::ChopSeparators() noexcept
{
	size_t l = length();
	const auto *p = data();

	while (l >= 2 && PathTraitsFS::IsSeparator(p[l - 1])) {
		--l;

		value.pop_back();
	}
}
