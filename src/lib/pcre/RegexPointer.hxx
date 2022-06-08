/*
 * Copyright 2007-2022 CM4all GmbH
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

#pragma once

#include "MatchData.hxx"

#include <pcre2.h>

#include <string_view>

class RegexPointer {
protected:
	pcre2_code_8 *re = nullptr;

	unsigned n_capture = 0;

public:
	constexpr bool IsDefined() const noexcept {
		return re != nullptr;
	}

	[[gnu::pure]]
	MatchData Match(std::string_view s) const noexcept {
		MatchData match_data{
			pcre2_match_data_create_from_pattern_8(re, nullptr),
			s.data(),
		};

		int n = pcre2_match_8(re, (PCRE2_SPTR8)s.data(), s.size(),
				      0, 0,
				      match_data.match_data, nullptr);
		if (n < 0)
			/* no match (or error) */
			return {};

		match_data.n = n;

		if (n_capture >= match_data.n)
			/* in its return value, PCRE omits mismatching
			   optional captures if (and only if) they are
			   the last capture; this kludge works around
			   this */
			match_data.n = n_capture + 1;

		return match_data;
	}
};
