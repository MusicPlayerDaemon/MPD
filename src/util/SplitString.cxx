// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SplitString.hxx"
#include "IterableSplitString.hxx"
#include "StringStrip.hxx"

std::forward_list<std::string_view>
SplitString(std::string_view s, char separator, bool strip) noexcept
{
	if (strip)
		s = StripLeft(s);

	std::forward_list<std::string_view> list;
	if (s.empty())
		return list;

	auto i = list.before_begin();

	for (auto value : IterableSplitString(s, separator)) {
		if (strip)
			value = Strip(value);

		i = list.emplace_after(i, value);
	}

	return list;
}
