// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StringFilter.hxx"
#include "util/StringAPI.hxx"

#include <cassert>

bool
StringFilter::MatchWithoutNegation(const char *s) const noexcept
{
	assert(s != nullptr);

#ifdef HAVE_PCRE
	if (regex)
		return regex->Match(s);
#endif

	if (fold_case) {
		switch (position) {
		case Position::FULL:
			break;

		case Position::ANYWHERE:
			return fold_case.IsIn(s);

		case Position::PREFIX:
			return fold_case.StartsWith(s);
		}

		return fold_case == s;
	} else {
		switch (position) {
		case Position::FULL:
			break;

		case Position::ANYWHERE:
			return StringFind(s, value.c_str()) != nullptr;

		case Position::PREFIX:
			return StringIsEqual(s, value.c_str(), value.length());
		}

		return value == s;
	}
}

bool
StringFilter::Match(const char *s) const noexcept
{
	return MatchWithoutNegation(s) != negated;
}
