// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ProtocolFeature.hxx"
#include "Client.hxx"
#include "Response.hxx"
#include "util/StringAPI.hxx"

#include <cassert>
#include <fmt/format.h>


struct feature_type_table {
	const char *name;

	ProtocolFeatureType type;
};

static constexpr struct feature_type_table protocol_feature_names_init[] = {
	{"hide_playlists_in_root", PF_HIDE_PLAYLISTS_IN_ROOT},
};

/**
 * This function converts the #tag_item_names_init array to an
 * associative array at compile time.  This is a kludge because C++20
 * doesn't support designated initializers for arrays, unlike C99.
 */
static constexpr auto
MakeProtocolFeatureNames() noexcept
{
	std::array<const char *, PF_NUM_OF_ITEM_TYPES> result{};

	static_assert(std::size(protocol_feature_names_init) == result.size());

	for (const auto &i : protocol_feature_names_init) {
		/* no duplicates allowed */
		assert(result[i.type] == nullptr);

		result[i.type] = i.name;
	}

	return result;
}

constinit const std::array<const char *, PF_NUM_OF_ITEM_TYPES> protocol_feature_names = MakeProtocolFeatureNames();

void
protocol_features_print(Client &client, Response &r) noexcept
{
	const auto protocol_feature = client.GetProtocolFeatures();
	for (unsigned i = 0; i < PF_NUM_OF_ITEM_TYPES; i++)
		if (protocol_feature.Test(ProtocolFeatureType(i)))
			r.Fmt(FMT_STRING("feature: {}\n"), protocol_feature_names[i]);
}

void
protocol_features_print_all(Response &r) noexcept
{
	for (unsigned i = 0; i < PF_NUM_OF_ITEM_TYPES; i++)
		r.Fmt(FMT_STRING("feature: {}\n"), protocol_feature_names[i]);
}

ProtocolFeatureType
protocol_feature_parse_i(const char *name) noexcept
{
	for (unsigned i = 0; i < PF_NUM_OF_ITEM_TYPES; ++i) {
		assert(protocol_feature_names[i] != nullptr);

		if (StringIsEqualIgnoreCase(name, protocol_feature_names[i]))
			return (ProtocolFeatureType)i;
	}

	return PF_NUM_OF_ITEM_TYPES;
}
