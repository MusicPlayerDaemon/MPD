// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string_view>
#include <cstdint>

class Client;
class Response;

/**
 * Codes for the string normalization options.
 */
enum StringNormalizationType : uint8_t {
	SN_STRIP_DIACRITICS,

	SN_NUM_OF_ITEM_TYPES
};

class StringNormalization {
	using string_normalization_t = uint_least8_t;

	/* must have enough bits to represent all string normalization options
	   supported by MPD */
	static_assert(SN_NUM_OF_ITEM_TYPES <= sizeof(string_normalization_t) * 8);

	string_normalization_t value;

	explicit constexpr StringNormalization(string_normalization_t  _value) noexcept
		:value(_value) {}

public:
	constexpr StringNormalization() noexcept = default;

	constexpr StringNormalization(StringNormalizationType _value) noexcept
		:value(string_normalization_t(1) << string_normalization_t(_value)) {}

	static constexpr StringNormalization None() noexcept {
		return StringNormalization(string_normalization_t(0));
	}

	static constexpr StringNormalization All() noexcept {
		return ~None();
	}

	constexpr StringNormalization operator~() const noexcept {
		return StringNormalization(~value);
	}

	constexpr StringNormalization operator&(StringNormalization other) const noexcept {
		return StringNormalization(value & other.value);
	}

	constexpr StringNormalization operator|(StringNormalization other) const noexcept {
		return StringNormalization(value | other.value);
	}

	constexpr StringNormalization operator^(StringNormalization other) const noexcept {
		return StringNormalization(value ^ other.value);
	}

	constexpr StringNormalization &operator&=(StringNormalization other) noexcept {
		value &= other.value;
		return *this;
	}

	constexpr StringNormalization &operator|=(StringNormalization other) noexcept {
		value |= other.value;
		return *this;
	}

	constexpr StringNormalization &operator^=(StringNormalization other) noexcept {
		value ^= other.value;
		return *this;
	}

	constexpr bool TestAny() const noexcept {
		return value != 0;
	}

	constexpr bool Test(StringNormalization feature) const noexcept {
		return (*this & feature).TestAny();
	}

	constexpr void Set(StringNormalization features) noexcept {
		*this |= features;
	}

	constexpr void Unset(StringNormalization features) noexcept {
		*this &= ~StringNormalization(features);
	}

	constexpr void SetAll() noexcept {
		*this = StringNormalization::All();
	}

	constexpr void Clear() noexcept {
		*this = StringNormalization::None();
	}
};

void
string_normalizations_print(Client &client, Response &r) noexcept;

void
string_normalizations_print_all(Response &r) noexcept;

StringNormalizationType
string_normalization_parse_i(const char *name) noexcept;
