/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "util/ConstBuffer.hxx"

class OptionDef;

/**
 * Command line option parser.
 */
class OptionParser
{
	ConstBuffer<const char *> args;
	const char *option;
	const char *option_raw;
	bool is_long = false;

	const char **const remaining_head, **remaining_tail;

public:
	/**
	 * Constructs #OptionParser.
	 */
	OptionParser(int _argc, char **_argv) noexcept
		:args(_argv + 1, _argc - 1),
		 remaining_head(const_cast<const char **>(_argv + 1)),
		 remaining_tail(remaining_head) {}

	/**
	 * Gets the last parsed option.
	 */
	const char *GetOption() noexcept {
		return option_raw;
	}

	/**
	 * Checks if current option is a specified option.
	 */
	bool CheckOption(const OptionDef &opt) const noexcept;

	/**
	 * Checks if current option is a specified option
	 * or specified alternative option.
	 */
	bool CheckOption(const OptionDef &opt,
			 const OptionDef &alt_opt) const noexcept {
		return CheckOption(opt) || CheckOption(alt_opt);
	}

	/**
	 * Parses current command line entry.
	 * Regardless of result, advances current position to the next
	 * command line entry. 
	 *
	 * @return true if an option was found, false if there are no
	 * more options
	 */
	bool ParseNext() noexcept;

	/**
	 * Returns the remaining non-option arguments.
	 */
	ConstBuffer<const char *> GetRemaining() const noexcept {
		return {remaining_head, remaining_tail};
	}
};

#endif
