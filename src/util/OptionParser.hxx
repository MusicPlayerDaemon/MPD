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

#ifndef MPD_UTIL_OPTIONPARSER_HXX
#define MPD_UTIL_OPTIONPARSER_HXX

#include <assert.h>

class OptionDef;

/**
 * Command line option parser.
 */
class OptionParser
{
	int argc;
	char **argv;
	char *option;
	char *option_raw;
	bool is_long;
public:
	/**
	 * Constructs #OptionParser.
	 */
	OptionParser(int _argc, char **_argv)
		: argc(_argc - 1), argv(_argv + 1),
		  option(nullptr), option_raw(nullptr), is_long(false) { }

	/**
	 * Checks if there are command line entries to process.
	 */
	bool HasEntries() const { return argc > 0; }

	/**
	 * Gets the last parsed option.
	 */
	char *GetOption() {
		assert(option_raw != nullptr);
		return option_raw;
	}

	/**
	 * Checks if current option is a specified option.
	 */
	bool CheckOption(const OptionDef& opt);

	/**
	 * Checks if current option is a specified option
	 * or specified alternative option.
	 */
	bool CheckOption(const OptionDef& opt, const OptionDef &alt_opt) {
		return CheckOption(opt) || CheckOption(alt_opt);
	}

	/**
	 * Parses current command line entry.
	 * Returns true on success, false otherwise.
	 * Regardless of result, advances current position to the next
	 * command line entry. 
	 */
	bool ParseNext();

	/**
	 * Checks if specified string is a command line option.
	 */
	static bool IsOption(const char *s) {
		assert(s != nullptr);
		return s[0] == '-';
	}
};

#endif
