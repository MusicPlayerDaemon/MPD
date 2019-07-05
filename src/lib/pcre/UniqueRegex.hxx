/*
 * Copyright 2007-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UNIQUE_REGEX_HXX
#define UNIQUE_REGEX_HXX

#include "RegexPointer.hxx"

#include <utility>

#include <pcre.h>

class UniqueRegex : public RegexPointer {
public:
	UniqueRegex() = default;

	UniqueRegex(const char *pattern, bool anchored, bool capture,
		    bool caseless) {
		Compile(pattern, anchored, capture, caseless);
	}

	UniqueRegex(UniqueRegex &&src) noexcept:RegexPointer(src) {
		src.re = nullptr;
		src.extra = nullptr;
	}

	~UniqueRegex() noexcept {
		pcre_free(re);
#ifdef PCRE_CONFIG_JIT
		pcre_free_study(extra);
#else
		pcre_free(extra);
#endif
	}

	UniqueRegex &operator=(UniqueRegex &&src) {
		using std::swap;
		swap<RegexPointer>(*this, src);
		return *this;
	}

	/**
	 * Throws std::runtime_error on error.
	 */
	void Compile(const char *pattern, bool anchored, bool capture,
		     bool caseless);
};

#endif
