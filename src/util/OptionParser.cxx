/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "OptionParser.hxx"
#include "OptionDef.hxx"
#include "util/RuntimeError.hxx"

#include <string.h>

inline OptionParser::Result
OptionParser::IdentifyOption(const char *s) const
{
	assert(s != nullptr);
	assert(*s == '-');

	if (s[1] == '-') {
		for (const auto &i : options)
			if (i.HasLongOption() &&
			    strcmp(s + 2, i.GetLongOption()) == 0)
				return {int(&i - options.data)};
	} else if (s[1] != 0 && s[2] == 0) {
		const char ch = s[1];
		for (const auto &i : options)
			if (i.HasShortOption() && ch == i.GetShortOption())
				return {int(&i - options.data)};
	}

	throw FormatRuntimeError("Unknown option: %s", s);
}

OptionParser::Result
OptionParser::Next()
{
	while (!args.empty()) {
		const char *arg = args.shift();
		if (arg[0] == '-')
			return IdentifyOption(arg);

		*remaining_tail++ = arg;
	}

	return {-1};
}
