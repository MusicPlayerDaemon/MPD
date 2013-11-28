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
#include "AllocatedPath.hxx"
#include "Domain.hxx"
#include "Charset.hxx"
#include "util/Error.hxx"
#include "Compiler.h"

#include <glib.h>

#include <string.h>

inline AllocatedPath::AllocatedPath(Donate, pointer _value)
	:value(_value) {
	g_free(_value);
}

/* no inlining, please */
AllocatedPath::~AllocatedPath() {}

AllocatedPath
AllocatedPath::Build(const_pointer a, const_pointer b)
{
	return AllocatedPath(Donate(), g_build_filename(a, b, nullptr));
}

AllocatedPath
AllocatedPath::FromUTF8(const char *path_utf8)
{
	return AllocatedPath(Donate(), ::PathFromUTF8(path_utf8));
}

AllocatedPath
AllocatedPath::FromUTF8(const char *path_utf8, Error &error)
{
	AllocatedPath path = FromUTF8(path_utf8);
	if (path.IsNull())
		error.Format(path_domain,
			     "Failed to convert to file system charset: %s",
			     path_utf8);

	return path;
}

AllocatedPath
AllocatedPath::GetDirectoryName() const
{
	return AllocatedPath(Donate(), g_path_get_dirname(c_str()));
}

std::string
AllocatedPath::ToUTF8() const
{
	return ::PathToUTF8(c_str());
}

const char *
AllocatedPath::RelativeFS(const char *other_fs) const
{
	const size_t l = length();
	if (memcmp(data(), other_fs, l) != 0)
		return nullptr;

	other_fs += l;
	if (*other_fs != 0) {
		if (!PathTraits::IsSeparatorFS(*other_fs))
			/* mismatch */
			return nullptr;

		/* skip remaining path separators */
		do {
			++other_fs;
		} while (PathTraits::IsSeparatorFS(*other_fs));
	}

	return other_fs;
}

void
AllocatedPath::ChopSeparators()
{
	size_t l = length();
	const char *p = data();

	while (l >= 2 && PathTraits::IsSeparatorFS(p[l - 1])) {
		--l;

#if GCC_CHECK_VERSION(4,7) && !defined(__clang__)
		value.pop_back();
#else
		value.erase(value.end() - 1, value.end());
#endif
	}
}
