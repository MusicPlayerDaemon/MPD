// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <array>
#include <string_view>

/**
 * A buffer holding a string with a length that is known at compile
 * time.  It is not null-terminated.
 */
template<std::size_t CAPACITY>
class FixedString : public std::array<char, CAPACITY> {
public:
	constexpr operator std::string_view() const noexcept {
		return {this->data(), this->size()};
	}
};
