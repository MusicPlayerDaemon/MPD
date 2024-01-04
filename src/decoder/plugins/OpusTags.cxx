// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

[[gnu::pure]]
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

		if (const auto i = ParseInteger<int_least32_t>(value))
			rgi->track.gain = float(*i) / 256.0f;
	} else if (rgi != nullptr &&
		   StringIsEqualIgnoreCase(name, "R128_ALBUM_GAIN"sv)) {
		/* R128_ALBUM_GAIN is a Q7.8 fixed point number in
		   dB */

		if (const auto i = ParseInteger<int_least32_t>(value))
			rgi->album.gain = float(*i) / 256.0f;
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
