// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ReservedName.hxx"
#include "util/CharUtil.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

using std::string_view_literals::operator""sv;

/**
 * Is this a digit suffix for Windows device names?
 *
 * @see https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
 */
static constexpr bool
IsWindowsDigit(std::string_view s) noexcept
{
	if (s.size() == 1)
		return IsDigitASCII(s.front());
	else if (s.size() == 2)
		/* Windows recognizes the 8-bit ISO/IEC 8859-1
		   superscript digits */
		return s == "\xc2\xb9"sv || s == "\xc2\xb2"sv || s == "\xc2\xb3"sv;
	else
		return false;
}

bool
IsWindowsReservedFilename(std::string_view name) noexcept
{
	// strip the suffix
	name = Split(name, '.').first;

	// strip trailing spaces
	name = StripRight(name);

	for (const auto i : {"CON"sv, "PRN"sv, "AUX"sv, "NUL"sv})
		if (StringIsEqualIgnoreCase(name, i))
			return true;

	for (const auto prefix : {"COM"sv, "LPT"sv}) {
		const auto rest = StringAfterPrefixIgnoreCase(name, prefix);
		if (IsWindowsDigit(rest))
			return true;
	}

	return false;
}
