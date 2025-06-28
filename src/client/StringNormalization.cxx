// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StringNormalization.hxx"
#include "Client.hxx"
#include "Response.hxx"
#include "util/StringAPI.hxx"

#include <cassert>
#include <fmt/format.h>

struct string_normalization_type_table {
	const char *name;

	StringNormalizationType type;
};

static constexpr struct string_normalization_type_table string_normalization_names_init[] = {
	{"strip_diacritics", SN_STRIP_DIACRITICS},
};

static constexpr auto
MakeStringNormalizationNames() noexcept
{
	std::array<const char *, PF_NUM_OF_ITEM_TYPES> result{};

	static_assert(std::size(string_normalization_names_init) == result.size());

	for (const auto &i : string_normalization_names_init) {
		assert(result[i.type] == nullptr);

		result[i.type] = i.name;
	}

	return result;
}

constinit const std::array<const char *, SN_NUM_OF_ITEM_TYPES> string_normalization_names = MakeStringNormalizationNames();

void
string_normalizations_print(Client &client, Response &r) noexcept
{
	const auto string_normalization = client.GetStringNormalizations();
	for (unsigned i = 0; i < PF_NUM_OF_ITEM_TYPES; i++)
		if (string_normalization.Test(StringNormalizationType(i)))
			r.Fmt("stringnormalization: {}\n", string_normalization_names[i]);
}

void
string_normalizations_print_all(Response &r) noexcept
{
	for (unsigned i = 0; i < SN_NUM_OF_ITEM_TYPES; i++)
		r.Fmt("stringnormalization: {}\n", string_normalization_names[i]);
}

StringNormalizationType
string_normalization_parse_i(const char *name) noexcept
{
	for (unsigned i = 0; i < SN_NUM_OF_ITEM_TYPES; ++i) {
		assert(string_normalization_names[i] != nullptr);

		if (StringIsEqualIgnoreCase(name, string_normalization_names[i]))
			return (StringNormalizationType)i;
	}

	return SN_NUM_OF_ITEM_TYPES;
}
