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

#include "config.h"
#include "ApeReplayGain.hxx"
#include "ApeLoader.hxx"
#include "ReplayGain.hxx"
#include "util/ASCII.hxx"
#include "fs/Path.hxx"

#include <string.h>
#include <stdlib.h>

static bool
replay_gain_ape_callback(unsigned long flags, const char *key,
			 const char *_value, size_t value_length,
			 ReplayGainInfo &info)
{
	/* we only care about utf-8 text tags */
	if ((flags & (0x3 << 1)) != 0)
		return false;

	char value[16];
	if (value_length >= sizeof(value))
		return false;

	memcpy(value, _value, value_length);
	value[value_length] = 0;

	return ParseReplayGainTag(info, key, value);
}

bool
replay_gain_ape_read(Path path_fs, ReplayGainInfo &info)
{
	bool found = false;

	auto callback = [&info, &found]
		(unsigned long flags, const char *key,
		 const char *value,
		 size_t value_length) {
		found |= replay_gain_ape_callback(flags, key,
						  value, value_length,
						  info);
		return true;
	};

	return tag_ape_scan(path_fs, callback) && found;
}
