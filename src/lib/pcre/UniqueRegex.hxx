// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "RegexPointer.hxx"

#include <utility>

namespace Pcre {

struct CompileOptions {
	bool anchored = false;
	bool caseless = false;
	bool capture = false;

	explicit constexpr operator int() const noexcept {
		int options = PCRE2_DOTALL|PCRE2_NO_AUTO_CAPTURE;

		if (anchored)
			options |= PCRE2_ANCHORED;

		if (caseless)
			options |= PCRE2_CASELESS;

		if (capture)
			options &= ~PCRE2_NO_AUTO_CAPTURE;

		return options;
	}
};

} // namespace Pcre

class UniqueRegex : public RegexPointer {
public:
	UniqueRegex() = default;

	UniqueRegex(const char *pattern, Pcre::CompileOptions options) {
		Compile(pattern, options);
	}

	UniqueRegex(UniqueRegex &&src) noexcept:RegexPointer(src) {
		src.re = nullptr;
	}

	~UniqueRegex() noexcept {
		if (re != nullptr)
			pcre2_code_free_8(re);
	}

	UniqueRegex &operator=(UniqueRegex &&src) noexcept {
		using std::swap;
		swap<RegexPointer>(*this, src);
		return *this;
	}

	/**
	 * Throws Pcre::Error on error.
	 */
	void Compile(const char *pattern, int options);

	void Compile(const char *pattern, Pcre::CompileOptions options={}) {
		Compile(pattern, (int)options);
	}
};
