/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "TagConfig.hxx"
#include "TagSettings.h"
#include "Tag.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "system/FatalError.hxx"
#include "util/Alloc.hxx"
#include "util/ASCII.hxx"
#include "util/StringUtil.hxx"

#include <algorithm>

#include <stdlib.h>

void
TagLoadConfig()
{
	const char *value = config_get_string(CONF_METADATA_TO_USE, nullptr);
	if (value == nullptr)
		return;

	std::fill_n(ignore_tag_items, size_t(TAG_NUM_OF_ITEM_TYPES), true);

	if (StringEqualsCaseASCII(value, "none"))
		return;

	bool quit = false;
	char *temp, *c, *s;
	temp = c = s = xstrdup(value);
	do {
		if (*s == ',' || *s == '\0') {
			if (*s == '\0')
				quit = true;
			*s = '\0';

			c = Strip(c);
			if (*c == 0)
				continue;

			const auto type = tag_name_parse_i(c);
			if (type == TAG_NUM_OF_ITEM_TYPES)
				FormatFatalError("error parsing metadata item \"%s\"",
						 c);

			ignore_tag_items[type] = false;

			s++;
			c = s;
		}
		s++;
	} while (!quit);

	free(temp);
}
