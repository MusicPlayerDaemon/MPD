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

#ifndef MPD_STRING_FILTER_HXX
#define MPD_STRING_FILTER_HXX

#include "lib/icu/Compare.hxx"
#include "config.h"

#ifdef HAVE_PCRE
#include "lib/pcre/UniqueRegex.hxx"
#endif

#include <string>
#include <memory>

class StringFilter {
	std::string value;

	/**
	 * This value is only set if case folding is enabled.
	 */
	IcuCompare fold_case;

#ifdef HAVE_PCRE
	std::shared_ptr<UniqueRegex> regex;
#endif

	/**
	 * Search for substrings instead of matching the whole string?
	 */
	bool substring;

	bool negated;

public:
	template<typename V>
	StringFilter(V &&_value, bool _fold_case, bool _substring, bool _negated)
		:value(std::forward<V>(_value)),
		 fold_case(_fold_case
			   ? IcuCompare(value)
			   : IcuCompare()),
		 substring(_substring), negated(_negated) {}

	bool empty() const noexcept {
		return value.empty();
	}

	bool IsRegex() const noexcept {
#ifdef HAVE_PCRE
		return !!regex;
#else
		return false;
#endif
	}

#ifdef HAVE_PCRE
	template<typename R>
	void SetRegex(R &&_regex) noexcept {
		regex = std::forward<R>(_regex);
	}
#endif

	const auto &GetValue() const noexcept {
		return value;
	}

	bool GetFoldCase() const noexcept {
		return fold_case;
	}

	bool IsNegated() const noexcept {
		return negated;
	}

	void ToggleNegated() noexcept {
		negated = !negated;
	}

	const char *GetOperator() const noexcept {
		return IsRegex()
			? (negated ? "!~" : "=~")
			: (substring
			   ? (negated ? "!contains" : "contains")
			   : (negated ? "!=" : "=="));
	}

	[[gnu::pure]]
	bool Match(const char *s) const noexcept;

	/**
	 * Like Match(), but ignore the "negated" flag.
	 */
	[[gnu::pure]]
	bool MatchWithoutNegation(const char *s) const noexcept;
};

#endif
