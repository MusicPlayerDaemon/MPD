/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "MimeType.hxx"
#include "IterableSplitString.hxx"
#include "StringView.hxx"

std::string_view
GetMimeTypeBase(std::string_view s) noexcept
{
	return StringView(s).Split(';').first;
}

std::map<std::string, std::string>
ParseMimeTypeParameters(std::string_view mime_type) noexcept
{
	/* discard the first segment (the base MIME type) */
	const auto params = StringView(mime_type).Split(';').second;

	std::map<std::string, std::string> result;
	for (auto i : IterableSplitString(params, ';')) {
		i.Strip();
		auto s = i.Split('=');
		if (!s.second.IsNull())
			result.emplace(s);
	}

	return result;
}
