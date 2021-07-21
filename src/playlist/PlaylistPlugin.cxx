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

#include "PlaylistPlugin.hxx"
#include "util/StringUtil.hxx"
#include "util/StringView.hxx"

bool
PlaylistPlugin::SupportsScheme(StringView scheme) const noexcept
{
	return schemes != nullptr &&
		StringArrayContainsCase(schemes, scheme);
}

bool
PlaylistPlugin::SupportsSuffix(StringView suffix) const noexcept
{
	return suffixes != nullptr &&
		StringArrayContainsCase(suffixes, suffix);
}

bool
PlaylistPlugin::SupportsMimeType(StringView mime_type) const noexcept
{
	return mime_types != nullptr &&
		StringArrayContainsCase(mime_types, mime_type);
}
