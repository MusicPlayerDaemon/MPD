/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "DecoderPlugin.hxx"
#include "util/StringUtil.hxx"

#include <assert.h>

bool
DecoderPlugin::SupportsSuffix(const char *suffix) const noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(suffix != nullptr);
#endif

	return suffixes != nullptr &&
		StringArrayContainsCase(suffixes, suffix);
}

bool
DecoderPlugin::SupportsMimeType(const char *mime_type) const noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(mime_type != nullptr);
#endif

	return mime_types != nullptr &&
		StringArrayContainsCase(mime_types, mime_type);
}
