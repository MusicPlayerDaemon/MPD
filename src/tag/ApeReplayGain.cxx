// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ApeReplayGain.hxx"
#include "ApeLoader.hxx"
#include "ReplayGainParser.hxx"

#include <algorithm>
#include <string_view>

#include <string.h>

static bool
replay_gain_ape_callback(unsigned long flags, const char *key,
			 std::string_view _value,
			 ReplayGainInfo &info)
{
	/* we only care about utf-8 text tags */
	if ((flags & (0x3 << 1)) != 0)
		return false;

	char value[16];
	if (_value.size() >= sizeof(value))
		return false;

	*std::copy(_value.begin(), _value.end(), value) = 0;

	return ParseReplayGainTag(info, key, value);
}

bool
replay_gain_ape_read(InputStream &is, ReplayGainInfo &info)
{
	bool found = false;

	auto callback = [&info, &found]
		(unsigned long flags, const char *key,
		 std::string_view value) {
		found |= replay_gain_ape_callback(flags, key,
						  value,
						  info);
		return true;
	};

	return tag_ape_scan(is, callback) && found;
}
