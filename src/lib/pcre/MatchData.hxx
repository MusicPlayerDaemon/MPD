/*
 * Copyright 2007-2021 CM4all GmbH
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

#include <pcre2.h>

#include <cassert>
#include <cstddef>
#include <string_view>
#include <utility>

class MatchData {
	friend class RegexPointer;

	pcre2_match_data_8 *match_data = nullptr;
	const char *s;
	PCRE2_SIZE *ovector;
	std::size_t n;

	explicit MatchData(pcre2_match_data_8 *_md, const char *_s) noexcept
		:match_data(_md), s(_s),
		 ovector(pcre2_get_ovector_pointer_8(match_data))
	{
	}

public:
	MatchData() = default;

	MatchData(MatchData &&src) noexcept
		:match_data(std::exchange(src.match_data, nullptr)),
		 s(src.s), ovector(src.ovector), n(src.n) {}

	~MatchData() noexcept {
		if (match_data != nullptr)
			pcre2_match_data_free_8(match_data);
	}

	MatchData &operator=(MatchData &&src) noexcept {
		using std::swap;
		swap(match_data, src.match_data);
		swap(s, src.s);
		swap(ovector, src.ovector);
		swap(n, src.n);
		return *this;
	}

	constexpr operator bool() const noexcept {
		return match_data != nullptr;
	}

	constexpr std::size_t size() const noexcept {
		assert(*this);

		return static_cast<std::size_t>(n);
	}

	[[gnu::pure]]
	constexpr std::string_view operator[](std::size_t i) const noexcept {
		assert(*this);
		assert(i < size());

		int start = ovector[2 * i];
		if (start < 0)
			return {};

		int end = ovector[2 * i + 1];
		assert(end >= start);

		return { s + start, std::size_t(end - start) };
	}

	static constexpr std::size_t npos = ~std::size_t{};

	[[gnu::pure]]
	constexpr std::size_t GetCaptureStart(std::size_t i) const noexcept {
		assert(*this);
		assert(i < size());

		int start = ovector[2 * i];
		if (start < 0)
			return npos;

		return std::size_t(start);
	}

	[[gnu::pure]]
	constexpr std::size_t GetCaptureEnd(std::size_t i) const noexcept {
		assert(*this);
		assert(i < size());

		int end = ovector[2 * i + 1];
		if (end < 0)
			return npos;

		return std::size_t(end);
	}
};
