// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UTIL_OPTIONPARSER_HXX
#define MPD_UTIL_OPTIONPARSER_HXX

#include "OptionDef.hxx"

#include <span>
#include <string_view>

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

	/**
	 * Peeks the value of a specified long option without advancing the parser state.
	 *
	 * @param longOption The long name of the option (without the leading "--").
	 * @return The value attached to the option, an empty string if the option is a flag,
	 *         or nullptr if the option is not present.
	 * @throws FmtRuntimeError if a value is expected but not provided.
	 */
	const char *PeekOptionValue(std::string_view s);

private:
	const char *CheckShiftValue(const char *s, const OptionDef &option);
	Result IdentifyOption(const char *s);
};

#endif
