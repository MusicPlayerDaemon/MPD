// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_MASK_HXX
#define MPD_TAG_MASK_HXX

#include <cstdint>

enum TagType : uint8_t;

class TagMask {
	typedef uint_least32_t mask_t;
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

#endif
