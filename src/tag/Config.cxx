// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Config.hxx"
#include "Settings.hxx"
#include "ParseName.hxx"
#include "Type.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/ASCII.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringCompare.hxx"
#include "util/StringStrip.hxx"

using std::string_view_literals::operator""sv;

void
TagLoadConfig(const ConfigData &config)
{
	const char *value = config.GetString(ConfigOption::METADATA_TO_USE);
	if (value == nullptr)
		return;

	if (StringEqualsCaseASCII(value, "none")) {
		global_tag_mask = TagMask::None();
		return;
	}

	bool plus = true;

	if (*value != '+' && *value != '-')
		/* no "+-": not incremental */
		global_tag_mask = TagMask::None();

	for (std::string_view name : IterableSplitString(value, ',')) {
		name = Strip(name);

		if (SkipPrefix(name, "+"sv)) {
			plus = true;
		} else if (SkipPrefix(name, "-"sv)) {
			plus = false;
		}

		const auto type = tag_name_parse_i(name);
		if (type == TAG_NUM_OF_ITEM_TYPES)
			throw FmtRuntimeError("error parsing metadata item {:?}",
					      name);

		if (plus)
			global_tag_mask.Set(type);
		else
			global_tag_mask.Unset(type);
	}
}
