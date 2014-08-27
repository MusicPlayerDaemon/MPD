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

#ifndef MPD_PROTOCOL_ARGPARSER_HXX
#define MPD_PROTOCOL_ARGPARSER_HXX

#include "check.h"

#include <stdint.h>

class Client;
class SongTime;
class SignedSongTime;

bool
check_uint32(Client &client, uint32_t *dst, const char *s);

bool
check_int(Client &client, int *value_r, const char *s);

bool
check_range(Client &client, unsigned *value_r1, unsigned *value_r2,
	    const char *s);

bool
check_unsigned(Client &client, unsigned *value_r, const char *s);

bool
check_bool(Client &client, bool *value_r, const char *s);

bool
check_float(Client &client, float *value_r, const char *s);

bool
ParseCommandArg(Client &client, SongTime &value_r, const char *s);

bool
ParseCommandArg(Client &client, SignedSongTime &value_r, const char *s);

#endif
