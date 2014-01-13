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

#ifndef MPD_UTIL_OPTIONDEF_HXX
#define MPD_UTIL_OPTIONDEF_HXX

/**
 * Command line option definition.
 */
class OptionDef
{
	const char *long_option;
	char short_option;
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

	bool HasLongOption() const { return long_option != nullptr; }
	bool HasShortOption() const { return short_option != 0; }
	bool HasDescription() const { return desc != nullptr; }

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
