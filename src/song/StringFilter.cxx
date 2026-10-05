// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StringFilter.hxx"
#include "util/StringAPI.hxx"

#include <cassert>

#ifdef HAVE_PCRE

namespace {

/**
 * A PCRE match context which limits the effort of each match,
 * because the patterns come from (untrusted) clients, and
 * pathological patterns could block the main thread for a very long
 * time.
 */
class FilterMatchContext {
	pcre2_match_context_8 *const context = pcre2_match_context_create_8(nullptr);

public:
	FilterMatchContext() noexcept {
		pcre2_set_match_limit_8(context, 10000);
		pcre2_set_depth_limit_8(context, 1000);
	}

	~FilterMatchContext() noexcept {
		pcre2_match_context_free_8(context);
	}

	FilterMatchContext(const FilterMatchContext &) = delete;
	FilterMatchContext &operator=(const FilterMatchContext &) = delete;

	operator pcre2_match_context_8 *() const noexcept {
		return context;
	}
};

} // anonymous namespace

#endif

bool
StringFilter::MatchWithoutNegation(const char *s) const noexcept
{
	assert(s != nullptr);

#ifdef HAVE_PCRE
	if (regex) {
		static const FilterMatchContext match_context;
		return regex->Match(s, match_context);
	}
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
