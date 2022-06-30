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

#include "Handler.hxx"
#include "Builder.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/CharUtil.hxx"
#include "util/StringCompare.hxx"
#include "util/StringView.hxx"

#include <algorithm>

using std::string_view_literals::operator""sv;

void
NullTagHandler::OnTag(TagType, std::string_view) noexcept
{
}

void
NullTagHandler::OnPair(std::string_view, std::string_view) noexcept
{
}

void
NullTagHandler::OnPicture(const char *, std::span<const std::byte>) noexcept
{
}

void
NullTagHandler::OnAudioFormat([[maybe_unused]] AudioFormat af) noexcept
{
}

void
AddTagHandler::OnDuration(SongTime duration) noexcept
{
	tag.SetDuration(duration);
}

/**
 * Skip leading zeroes and a non-decimal suffix.
 */
static std::string_view
NormalizeDecimal(std::string_view s)
{
	auto start = std::find_if(s.begin(), s.end(),
				  [](char ch){ return ch != '0'; });
	auto end = std::find_if(start, s.end(),
				[](char ch){ return !IsDigitASCII(ch); });
	return StringView{start, end};
}

void
AddTagHandler::OnTag(TagType type, std::string_view value) noexcept
{
	if (type == TAG_TRACK || type == TAG_DISC) {
		/* filter out this extra data and leading zeroes */

		value = NormalizeDecimal(value);
	}

	tag.AddItem(type, value);
}

void
FullTagHandler::OnPair(std::string_view name, std::string_view) noexcept
{
	if (StringIsEqualIgnoreCase(name, "cuesheet"sv))
		tag.SetHasPlaylist(true);
}

void
FullTagHandler::OnAudioFormat(AudioFormat af) noexcept
{
	if (audio_format != nullptr)
		*audio_format = af;
}
