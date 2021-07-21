/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "Config.hxx"
#include "Settings.hxx"
#include "ParseName.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "util/ASCII.hxx"
#include "util/RuntimeError.hxx"
#include "util/SplitString.hxx"
#include "util/StringView.hxx"

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

	for (StringView name : SplitString(value, ',')) {
		if (name.SkipPrefix("+")) {
			plus = true;
		} else if (name.SkipPrefix("-")) {
			plus = false;
		}

		const auto type = tag_name_parse_i(name);
		if (type == TAG_NUM_OF_ITEM_TYPES)
			throw FormatRuntimeError("error parsing metadata item \"%s\"",
						 name);

		if (plus)
			global_tag_mask.Set(type);
		else
			global_tag_mask.Unset(type);
	}
}
