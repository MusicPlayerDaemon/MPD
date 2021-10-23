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

#include "AllowedFormat.hxx"
#include "pcm/AudioParser.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringBuffer.hxx"
#include "config.h"

#include <stdexcept>

namespace Alsa {

AllowedFormat::AllowedFormat(StringView s)
{
#ifdef ENABLE_DSD
	dop = s.RemoveSuffix("=dop");
#endif

	char buffer[64];
	if (s.size >= sizeof(buffer))
		throw std::invalid_argument("Failed to parse audio format");

	memcpy(buffer, s.data, s.size);
	buffer[s.size] = 0;

	format = ParseAudioFormat(buffer, true);

#ifdef ENABLE_DSD
	if (dop && format.format != SampleFormat::DSD)
		throw std::invalid_argument("DoP works only with DSD");
#endif
}

std::forward_list<AllowedFormat>
AllowedFormat::ParseList(std::string_view s)
{
	std::forward_list<AllowedFormat> list;
	auto tail = list.before_begin();

	for (const auto i : IterableSplitString(s, ' '))
		if (!i.empty())
			tail = list.emplace_after(tail, i);

	return list;
}

std::string
ToString(const std::forward_list<AllowedFormat> &allowed_formats) noexcept
{
	std::string result;

	for (const auto &i : allowed_formats) {
		if (!result.empty())
			result.push_back(' ');

		result += ::ToString(i.format);

#ifdef ENABLE_DSD
		if (i.dop)
			result += "=dop";
#endif
	}

	return result;
}

} // namespace Alsa
