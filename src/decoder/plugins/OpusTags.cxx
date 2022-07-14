/*
 * Copyright 2003-2022 The Music Player Daemon Project
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
#include "lib/xiph/VorbisPicture.hxx"
#include "lib/xiph/XiphTags.hxx"
#include "tag/Handler.hxx"
#include "tag/ParseName.hxx"
#include "util/ASCII.hxx"
#include "tag/ReplayGainInfo.hxx"
#include "util/NumberParser.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"

#include <cstdint>

using std::string_view_literals::operator""sv;

gcc_pure
static TagType
ParseOpusTagName(std::string_view name) noexcept
{
	TagType type = tag_name_parse_i(name);
	if (type != TAG_NUM_OF_ITEM_TYPES)
		return type;

	return tag_table_lookup_i(xiph_tags, name);
}

static void
ScanOneOpusTag(std::string_view name, std::string_view value,
	       ReplayGainInfo *rgi,
	       TagHandler &handler) noexcept
{
	if (handler.WantPicture() &&
	    StringIsEqualIgnoreCase(name, "METADATA_BLOCK_PICTURE"sv))
		return ScanVorbisPicture(value, handler);

	if (value.size() >= 4096)
		/* ignore large values */
		return;

	if (rgi != nullptr &&
	    StringIsEqualIgnoreCase(name, "R128_TRACK_GAIN"sv)) {
		/* R128_TRACK_GAIN is a Q7.8 fixed point number in
		   dB */

		const char *endptr;
		const auto l = ParseInt64(value, &endptr, 10);
		if (endptr > value.begin() && endptr == value.end())
			rgi->track.gain = float(l) / 256.0f;
	} else if (rgi != nullptr &&
		   StringIsEqualIgnoreCase(name, "R128_ALBUM_GAIN"sv)) {
		/* R128_ALBUM_GAIN is a Q7.8 fixed point number in
		   dB */

		const char *endptr;
		const auto l = ParseInt64(value, &endptr, 10);
		if (endptr > value.begin() && endptr == value.end())
			rgi->album.gain = float(l) / 256.0f;
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

	if (!handler.WantPair() && !handler.WantTag() &&
	    !handler.WantPicture())
		return true;

	if (!r.SkipString())
		return false;

	uint32_t n;
	if (!r.ReadWord(n))
		return false;

	while (n-- > 0) {
		const auto s = r.ReadString();
		if (s.data() == nullptr)
			return false;

		const auto split = Split(s, '=');
		if (split.first.empty() || split.second.data() == nullptr)
			continue;

		ScanOneOpusTag(split.first, split.second, rgi, handler);
	}

	return true;
}
