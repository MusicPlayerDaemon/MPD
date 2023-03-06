// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STRING_FILTER_HXX
#define MPD_STRING_FILTER_HXX

#include "lib/icu/Compare.hxx"
#include "config.h"

#ifdef HAVE_PCRE
#include "lib/pcre/UniqueRegex.hxx"
#endif

#include <cstdint>
#include <string>
#include <memory>

class StringFilter {
public:
	enum class Position : uint_least8_t {
		/** compare the whole haystack */
		FULL,

		/** find the phrase anywhere in the haystack */
		ANYWHERE,

		/** check if the haystack starts with the given prefix */
		PREFIX,
	};

private:
	std::string value;

	/**
	 * This value is only set if case folding is enabled.
	 */
	IcuCompare fold_case;

#ifdef HAVE_PCRE
	std::shared_ptr<UniqueRegex> regex;
#endif

	Position position;

	bool negated;

public:
	template<typename V>
	StringFilter(V &&_value, bool _fold_case, Position _position, bool _negated)
		:value(std::forward<V>(_value)),
		 fold_case(_fold_case
			   ? IcuCompare(value)
			   : IcuCompare()),
		 position(_position),
		 negated(_negated) {}

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
		if (IsRegex())
			return negated ? "!~" : "=~";

		switch (position) {
		case Position::FULL:
			break;

		case Position::ANYWHERE:
			return negated ? "!contains" : "contains";

		case Position::PREFIX:
			return negated ? "!starts_with" : "starts_with";
		}

		return negated ? "!=" : "==";
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
