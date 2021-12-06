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

#include "ApeReplayGain.hxx"
#include "ApeLoader.hxx"
#include "ReplayGainParser.hxx"
#include "util/StringView.hxx"

#include <string.h>

static bool
replay_gain_ape_callback(unsigned long flags, const char *key,
			 StringView _value,
			 ReplayGainInfo &info)
{
	/* we only care about utf-8 text tags */
	if ((flags & (0x3 << 1)) != 0)
		return false;

	char value[16];
	if (_value.size >= sizeof(value))
		return false;

	memcpy(value, _value.data, _value.size);
	value[_value.size] = 0;

	return ParseReplayGainTag(info, key, value);
}

bool
replay_gain_ape_read(InputStream &is, ReplayGainInfo &info)
{
	bool found = false;

	auto callback = [&info, &found]
		(unsigned long flags, const char *key,
		 StringView value) {
		found |= replay_gain_ape_callback(flags, key,
						  value,
						  info);
		return true;
	};

	return tag_ape_scan(is, callback) && found;
}
