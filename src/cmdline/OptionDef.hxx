// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UTIL_OPTIONDEF_HXX
#define MPD_UTIL_OPTIONDEF_HXX

#include <cassert>

/**
 * Command line option definition.
 */
class OptionDef
{
	const char *long_option;
	char short_option;
	bool has_value = false;
	const char *desc;
public:
	constexpr OptionDef(const char *_long_option, const char *_desc)
		: long_option(_long_option),
		  short_option(0),
		  desc(_desc) { }

	constexpr OptionDef(const char *_long_option,
			    char _short_option, const char *_desc)
		: long_option(_long_option),
		  short_option(_short_option),
		  desc(_desc) { }

	constexpr OptionDef(const char *_long_option,
			    char _short_option, bool _has_value,
			    const char *_desc) noexcept
		:long_option(_long_option),
		 short_option(_short_option),
		 has_value(_has_value),
		 desc(_desc) {}

	constexpr bool HasLongOption() const { return long_option != nullptr; }
	constexpr bool HasShortOption() const { return short_option != 0; }

	constexpr bool HasValue() const noexcept {
		return has_value;
	}

	constexpr bool HasDescription() const { return desc != nullptr; }

	const char *GetLongOption() const {
		assert(HasLongOption());
		return long_option;
	}

	char GetShortOption() const {
		assert(HasShortOption());
		return short_option;
	}

	const char *GetDescription() const {
		assert(HasDescription());
		return desc;
	}
};

#endif
