// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AllowedFormat.hxx"
#include "pcm/AudioParser.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringBuffer.hxx"
#include "util/StringCompare.hxx"
#include "config.h"

#include <stdexcept>

using std::string_view_literals::operator""sv;

namespace Alsa {

AllowedFormat::AllowedFormat(std::string_view s)
{
#ifdef ENABLE_DSD
	dop = RemoveSuffix(s, "=dop"sv);
#endif

	char buffer[64];
	if (s.size() >= sizeof(buffer))
		throw std::invalid_argument("Failed to parse audio format");

	*std::copy(s.begin(), s.end(), buffer) = 0;

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
