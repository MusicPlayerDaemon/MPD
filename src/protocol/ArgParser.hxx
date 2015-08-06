/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#ifndef MPD_PROTOCOL_ARGPARSER_HXX
#define MPD_PROTOCOL_ARGPARSER_HXX

#include "check.h"

#include <limits>

#include <stdint.h>

class Response;
class SongTime;
class SignedSongTime;

bool
ParseCommandArg32(Response &r, uint32_t &value_r, const char *s);

bool
ParseCommandArg(Response &r, int &value_r, const char *s,
		int min_value, int max_value);

bool
ParseCommandArg(Response &r, int &value_r, const char *s);

struct RangeArg {
	unsigned start, end;

	void SetAll() {
		start = 0;
		end = std::numeric_limits<unsigned>::max();
	}

	static constexpr RangeArg All() {
		return { 0, std::numeric_limits<unsigned>::max() };
	}
};

bool
ParseCommandArg(Response &r, RangeArg &value_r, const char *s);

bool
ParseCommandArg(Response &r, unsigned &value_r, const char *s,
		unsigned max_value);

bool
ParseCommandArg(Response &r, unsigned &value_r, const char *s);

bool
ParseCommandArg(Response &r, bool &value_r, const char *s);

bool
ParseCommandArg(Response &r, float &value_r, const char *s);

bool
ParseCommandArg(Response &r, SongTime &value_r, const char *s);

bool
ParseCommandArg(Response &r, SignedSongTime &value_r, const char *s);

#endif
