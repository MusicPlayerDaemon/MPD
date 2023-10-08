// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Type.hxx"

#include <cstdint>

class TagMask {
	using mask_t = uint_least64_t;

	/* the mask must have enough bits to represent all tags
	   supported by MPD */
	static_assert(TAG_NUM_OF_ITEM_TYPES <= sizeof(mask_t) * 8);

	mask_t value;

	explicit constexpr TagMask(uint_least32_t _value) noexcept
		:value(_value) {}

public:
	TagMask() = default;

	constexpr TagMask(TagType tag) noexcept
		:value(mask_t(1) << mask_t(tag)) {}

	static constexpr TagMask None() noexcept {
		return TagMask(mask_t(0));
	}

	static constexpr TagMask All() noexcept {
		return ~None();
	}

	constexpr TagMask operator~() const noexcept {
		return TagMask(~value);
	}

	constexpr TagMask operator&(TagMask other) const noexcept {
		return TagMask(value & other.value);
	}

	constexpr TagMask operator|(TagMask other) const noexcept {
		return TagMask(value | other.value);
	}

	constexpr TagMask operator^(TagMask other) const noexcept {
		return TagMask(value ^ other.value);
	}

	TagMask &operator&=(TagMask other) noexcept {
		value &= other.value;
		return *this;
	}

	TagMask &operator|=(TagMask other) noexcept {
		value |= other.value;
		return *this;
	}

	TagMask &operator^=(TagMask other) noexcept {
		value ^= other.value;
		return *this;
	}

	constexpr bool TestAny() const noexcept {
		return value != 0;
	}

	constexpr bool Test(TagType tag) const noexcept {
		return (*this & tag).TestAny();
	}

	void Set(TagType tag) noexcept {
		*this |= tag;
	}

	void Unset(TagType tag) noexcept {
		*this &= ~TagMask(tag);
	}
};
