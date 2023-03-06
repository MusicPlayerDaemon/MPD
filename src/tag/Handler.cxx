// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Handler.hxx"
#include "Builder.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/CharUtil.hxx"
#include "util/StringCompare.hxx"

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
	return std::string_view{start, std::size_t(std::distance(start, end))};
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
