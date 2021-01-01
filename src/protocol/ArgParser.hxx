/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include <cstdint>

struct RangeArg;
class SongTime;
class SignedSongTime;

uint32_t
ParseCommandArgU32(const char *s);

int
ParseCommandArgInt(const char *s, int min_value, int max_value);

int
ParseCommandArgInt(const char *s);

RangeArg
ParseCommandArgRange(const char *s);

unsigned
ParseCommandArgUnsigned(const char *s, unsigned max_value);

unsigned
ParseCommandArgUnsigned(const char *s);

bool
ParseCommandArgBool(const char *s);

float
ParseCommandArgFloat(const char *s);

SongTime
ParseCommandArgSongTime(const char *s);

SignedSongTime
ParseCommandArgSignedSongTime(const char *s);

#endif
