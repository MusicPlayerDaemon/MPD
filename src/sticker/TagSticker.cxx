// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TagSticker.hxx"
#include "Database.hxx"
#include "AllowedTags.hxx"
#include "db/Interface.hxx"
#include "db/Selection.hxx"
#include "tag/Mask.hxx"
#include "tag/Names.hxx"
#include "tag/ParseName.hxx"
#include "song/Filter.hxx"
#include "util/StringAPI.hxx"

SongFilter
MakeSongFilter(const char *filter_string)
{
	const std::array args{filter_string};

	auto filter = SongFilter();
	filter.Parse(args, false);
	filter.Optimize();

	return filter;
}

SongFilter
MakeSongFilter(TagType tag_type, const char *tag_value)
{
	if (!sticker_allowed_tags.Test(tag_type))
		throw std::runtime_error("tag type not allowed for sticker");

	const std::array args{tag_item_names[tag_type], tag_value};

	SongFilter filter;
	filter.Parse(args, false);
	filter.Optimize();
	return filter;
}

SongFilter
MakeSongFilter(const char *sticker_type, const char *sticker_uri)
{
	if (StringIsEqual(sticker_type, "filter"))
		return MakeSongFilter(sticker_uri);

	if (auto tag_type = tag_name_parse_i(sticker_type); tag_type != TAG_NUM_OF_ITEM_TYPES)
		return MakeSongFilter(tag_type, sticker_uri);

	return {};
}

SongFilter
MakeSongFilterNoThrow(const char *sticker_type, const char *sticker_uri) noexcept
{
	try {
		return MakeSongFilter(sticker_type, sticker_uri);
	} catch (...) {
		return {};
	}
}

bool
TagExists(const Database &database, TagType tag_type, const char *tag_value)
{
	return FilterMatches(database, MakeSongFilter(tag_type, tag_value));
}

bool
FilterMatches(const Database &database, const SongFilter &filter) noexcept
{
	if (filter.IsEmpty())
		return false;

	const DatabaseSelection selection{"", true, &filter};
	
	// TODO: we just need to know if the tag selection has a match.
	//       a visitor callback that can stop the db visit by return value
	//       may be cleaner than throwing an exception.
	struct MatchFoundException {};
	try {
		database.Visit(selection, [](const LightSong &) {
			throw MatchFoundException{};
		});
	} catch (MatchFoundException) {
		return true;
	}

	return false;
}
