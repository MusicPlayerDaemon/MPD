/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "Compiler.h"

#include <limits>

#include <stdint.h>

class Response;
class SongTime;
class SignedSongTime;

gcc_pure
uint32_t
ParseCommandArgU32(const char *s);

gcc_pure
int
ParseCommandArgInt(const char *s, int min_value, int max_value);

gcc_pure
int
ParseCommandArgInt(const char *s);

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

gcc_pure
RangeArg
ParseCommandArgRange(const char *s);

gcc_pure
unsigned
ParseCommandArgUnsigned(const char *s, unsigned max_value);

gcc_pure
unsigned
ParseCommandArgUnsigned(const char *s);

gcc_pure
bool
ParseCommandArgBool(const char *s);

gcc_pure
float
ParseCommandArgFloat(const char *s);

gcc_pure
SongTime
ParseCommandArgSongTime(const char *s);

gcc_pure
SignedSongTime
ParseCommandArgSignedSongTime(const char *s);

#endif
