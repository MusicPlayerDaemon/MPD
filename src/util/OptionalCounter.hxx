/*
 * Copyright 2022 CM4all GmbH
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

#include <cassert>
#include <cstddef>

template<bool enable> class OptionalCounter;

template<>
class OptionalCounter<false>
{
public:
	constexpr void reset() noexcept {}
	constexpr auto &operator++() noexcept { return *this; }
	constexpr auto &operator--() noexcept { return *this; }
	constexpr auto &operator+=(std::size_t) noexcept { return *this; }
	constexpr auto &operator-=(std::size_t) noexcept { return *this; }
};

template<>
class OptionalCounter<true>
{
	std::size_t value = 0;

public:
	constexpr operator std::size_t() const noexcept {
		return value;
	}

	constexpr void reset() noexcept {
		value = 0;
	}

	constexpr auto &operator++() noexcept {
		++value;
		return *this;
	}

	constexpr auto &operator--() noexcept {
		assert(value > 0);

		--value;
		return *this;
	}

	constexpr auto &operator+=(std::size_t n) noexcept {
		value += n;
		return *this;
	}

	constexpr auto &operator-=(std::size_t n) noexcept {
		assert(value >= n);

		value -= n;
		return *this;
	}
};
