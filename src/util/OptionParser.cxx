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

#include "OptionParser.hxx"
#include "OptionDef.hxx"

#include <string.h>

bool OptionParser::CheckOption(const OptionDef &opt)
{
	assert(option != nullptr);

	if (is_long)
		return opt.HasLongOption() &&
		       strcmp(option, opt.GetLongOption()) == 0;

	return opt.HasShortOption() &&
	       option[0] == opt.GetShortOption() &&
	       option[1] == '\0';
}

bool OptionParser::ParseNext()
{
	assert(HasEntries());
	char *arg = *argv;
	++argv;
	--argc;
	if (arg[0] == '-') {
		if (arg[1] == '-') {
			option = arg + 2;
			is_long = true;
		}
		else {
			option = arg + 1;
			is_long = false;
		}
		option_raw = arg;
		return true;
	}
	option = nullptr;
	option_raw = nullptr;
	return false;
}
