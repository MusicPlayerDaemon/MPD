// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ParseName.hxx"
#include "Names.hxx"
#include "util/ASCII.hxx"
#include "util/StringCompare.hxx"

#include <cassert>

#include <string.h>

TagType
tag_name_parse(std::string_view name) noexcept
{
	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		assert(tag_item_names[i] != nullptr);

		if (name == tag_item_names[i])
			return (TagType)i;
	}

	return TAG_NUM_OF_ITEM_TYPES;
}

TagType
tag_name_parse_i(const char *name) noexcept
{
	assert(name != nullptr);

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		assert(tag_item_names[i] != nullptr);

		if (StringEqualsCaseASCII(name, tag_item_names[i]))
			return (TagType)i;
	}

	return TAG_NUM_OF_ITEM_TYPES;
}

TagType
tag_name_parse_i(std::string_view name) noexcept
{
	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		assert(tag_item_names[i] != nullptr);

		if (StringIsEqualIgnoreCase(name, tag_item_names[i]))
			return (TagType)i;
	}

	return TAG_NUM_OF_ITEM_TYPES;
}
