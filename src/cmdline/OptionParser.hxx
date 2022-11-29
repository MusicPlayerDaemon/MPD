/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "OptionDef.hxx"

#include <span>

/**
 * Command line option parser.
 */
class OptionParser
{
	std::span<const OptionDef> options;

	std::span<const char *const> args;

	const char **const remaining_head, **remaining_tail;

public:
	/**
	 * Constructs #OptionParser.
	 */
	OptionParser(std::span<const OptionDef> _options,
		     int _argc, char **_argv) noexcept
		:options(_options), args(_argv + 1, _argc - 1),
		 remaining_head(const_cast<const char **>(_argv + 1)),
		 remaining_tail(remaining_head) {}

	struct Result {
		int index;
		const char *value;

		constexpr operator bool() const noexcept {
			return index >= 0;
		}
	};

	/**
	 * Parses current command line entry.
	 * Regardless of result, advances current position to the next
	 * command line entry. 
	 *
	 * Throws on error.
	 */
	Result Next();

	/**
	 * Returns the remaining non-option arguments.
	 */
	std::span<const char *const> GetRemaining() const noexcept {
		return {remaining_head, remaining_tail};
	}

private:
	const char *CheckShiftValue(const char *s, const OptionDef &option);
	Result IdentifyOption(const char *s);
};

#endif
