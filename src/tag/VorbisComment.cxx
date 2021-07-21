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

#include "VorbisComment.hxx"
#include "util/StringView.hxx"

#include <cassert>

StringView
GetVorbisCommentValue(StringView entry, StringView name) noexcept
{
	assert(!name.empty());

	if (entry.StartsWithIgnoreCase(name) &&
	    entry.size > name.size &&
	    entry[name.size] == '=') {
		entry.skip_front(name.size + 1);
		return entry;
	}

	return nullptr;
}
