/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "OpusTags.hxx"
#include "OpusReader.hxx"
#include "lib/xiph/XiphTags.hxx"
#include "tag/Handler.hxx"
#include "tag/ParseName.hxx"
#include "util/ASCII.hxx"
#include "ReplayGainInfo.hxx"

#include <string>

#include <stdint.h>
#include <stdlib.h>

gcc_pure
static TagType
ParseOpusTagName(const char *name) noexcept
{
	TagType type = tag_name_parse_i(name);
	if (type != TAG_NUM_OF_ITEM_TYPES)
		return type;

	return tag_table_lookup_i(xiph_tags, name);
}

static void
ScanOneOpusTag(const char *name, const char *value,
	       ReplayGainInfo *rgi,
	       TagHandler &handler) noexcept
{
	if (rgi != nullptr && StringEqualsCaseASCII(name, "R128_TRACK_GAIN")) {
		/* R128_TRACK_GAIN is a Q7.8 fixed point number in
		   dB */

		char *endptr;
		long l = strtol(value, &endptr, 10);
		if (endptr > value && *endptr == 0)
			rgi->track.gain = double(l) / 256.;
	} else if (rgi != nullptr &&
		   StringEqualsCaseASCII(name, "R128_ALBUM_GAIN")) {
		/* R128_ALBUM_GAIN is a Q7.8 fixed point number in
		   dB */

		char *endptr;
		long l = strtol(value, &endptr, 10);
		if (endptr > value && *endptr == 0)
			rgi->album.gain = double(l) / 256.;
	}

	handler.OnPair(name, value);

	if (handler.WantTag()) {
		TagType t = ParseOpusTagName(name);
		if (t != TAG_NUM_OF_ITEM_TYPES)
			handler.OnTag(t, value);
	}
}

bool
ScanOpusTags(const void *data, size_t size,
	     ReplayGainInfo *rgi,
	     TagHandler &handler) noexcept
{
	OpusReader r(data, size);
	if (!r.Expect("OpusTags", 8))
		return false;

	if (!handler.WantPair() && !handler.WantTag())
		return true;

	if (!r.SkipString())
		return false;

	uint32_t n;
	if (!r.ReadWord(n))
		return false;

	while (n-- > 0) {
		const auto s = r.ReadString();
		if (s == nullptr)
			return false;

		if (s.size >= 4096)
			continue;

		const auto eq = s.Find('=');
		if (eq == nullptr || eq == s.data)
			continue;

		auto name = s, value = s;
		name.SetEnd(eq);
		value.MoveFront(eq + 1);

		const std::string name2(name.data, name.size);
		const std::string value2(value.data, value.size);

		ScanOneOpusTag(name2.c_str(), value2.c_str(), rgi, handler);
	}

	return true;
}
