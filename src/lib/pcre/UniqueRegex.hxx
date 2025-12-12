// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Options.hxx"
#include "RegexPointer.hxx"

#include <string_view>
#include <utility>

class UniqueRegex : public RegexPointer {
public:
	UniqueRegex() = default;

	UniqueRegex(const char *pattern, Pcre::CompileOptions options) {
		Compile(pattern, options);
	}

	UniqueRegex(std::string_view pattern, Pcre::CompileOptions options) {
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
	void Compile(std::string_view pattern, int options);

	void Compile(const char *pattern, Pcre::CompileOptions options={}) {
		Compile(pattern, (int)options);
	}

	void Compile(std::string_view pattern, Pcre::CompileOptions options={}) {
		Compile(pattern, (int)options);
	}
};
