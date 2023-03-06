// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Escape.hxx"

static constexpr bool
MustEscape(char ch) noexcept
{
	return ch == '"' || ch == '\'' || ch == '\\';
}

std::string
EscapeFilterString(const std::string &src) noexcept
{
	std::string result;
	result.reserve(src.length() + 16);

	for (char ch : src) {
		if (MustEscape(ch))
			result.push_back('\\');
		result.push_back(ch);
	}

	return result;
}
