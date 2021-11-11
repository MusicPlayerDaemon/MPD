/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "Charset.hxx"
#include "Features.hxx"

/* no inlining, please */
AllocatedPath::~AllocatedPath() noexcept = default;

AllocatedPath
AllocatedPath::FromUTF8(std::string_view path_utf8) noexcept
{
#ifdef FS_CHARSET_ALWAYS_UTF8
	return FromFS(path_utf8);
#else
	try {
		return {::PathFromUTF8(path_utf8)};
	} catch (...) {
		return nullptr;
	}
#endif
}

AllocatedPath
AllocatedPath::FromUTF8Throw(std::string_view path_utf8)
{
#ifdef FS_CHARSET_ALWAYS_UTF8
	return FromFS(path_utf8);
#else
	return {::PathFromUTF8(path_utf8)};
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
