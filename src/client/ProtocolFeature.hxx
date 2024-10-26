// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string_view>
#include <cstdint>

class Client;
class Response;

/**
 * Codes for the type of a protocol feature.
 */
enum ProtocolFeatureType : uint8_t {
	PF_HIDE_PLAYLISTS_IN_ROOT,

	PF_NUM_OF_ITEM_TYPES
};

class ProtocolFeature {
	using protocol_feature_t = uint_least8_t;

	/* must have enough bits to represent all protocol features
	   supported by MPD */
	static_assert(PF_NUM_OF_ITEM_TYPES <= sizeof(protocol_feature_t) * 8);

	protocol_feature_t value;

	explicit constexpr ProtocolFeature(protocol_feature_t _value) noexcept
		:value(_value) {}

public:
	constexpr ProtocolFeature() noexcept = default;

	constexpr ProtocolFeature(ProtocolFeatureType _value) noexcept
		:value(protocol_feature_t(1) << protocol_feature_t(_value)) {}

	static constexpr ProtocolFeature None() noexcept {
		return ProtocolFeature(protocol_feature_t(0));
	}

	static constexpr ProtocolFeature All() noexcept {
		return ~None();
	}

	constexpr ProtocolFeature operator~() const noexcept {
		return ProtocolFeature(~value);
	}

	constexpr ProtocolFeature operator&(ProtocolFeature other) const noexcept {
		return ProtocolFeature(value & other.value);
	}

	constexpr ProtocolFeature operator|(ProtocolFeature other) const noexcept {
		return ProtocolFeature(value | other.value);
	}

	constexpr ProtocolFeature operator^(ProtocolFeature other) const noexcept {
		return ProtocolFeature(value ^ other.value);
	}

	constexpr ProtocolFeature &operator&=(ProtocolFeature other) noexcept {
		value &= other.value;
		return *this;
	}

	constexpr ProtocolFeature &operator|=(ProtocolFeature other) noexcept {
		value |= other.value;
		return *this;
	}

	constexpr ProtocolFeature &operator^=(ProtocolFeature other) noexcept {
		value ^= other.value;
		return *this;
	}

	constexpr bool TestAny() const noexcept {
		return value != 0;
	}

	constexpr bool Test(ProtocolFeatureType feature) const noexcept {
		return (*this & feature).TestAny();
	}

	constexpr void Set(ProtocolFeature features) noexcept {
		*this |= features;
	}

	constexpr void Unset(ProtocolFeature features) noexcept {
		*this &= ~ProtocolFeature(features);
	}

	constexpr void SetAll() noexcept {
		*this = ProtocolFeature::All();
	}

	constexpr void Clear() noexcept {
		*this = ProtocolFeature::None();
	}
};

void
protocol_features_print(Client &client, Response &r) noexcept;

void
protocol_features_print_all(Response &r) noexcept;

ProtocolFeatureType
protocol_feature_parse_i(const char *name) noexcept;
