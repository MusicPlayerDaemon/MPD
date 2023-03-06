// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
