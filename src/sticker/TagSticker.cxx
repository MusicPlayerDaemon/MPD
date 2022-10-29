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

#include "TagSticker.hxx"

#include "tag/Type.h"
#include "db/Interface.hxx"
#include "db/Selection.hxx"
#include "tag/Tag.hxx"
#include "tag/ParseName.hxx"
#include "song/LightSong.hxx"
#include "song/Filter.hxx"
#include "sticker/Database.hxx"
#include "filter/Filter.hxx"
#include "AllowedTags.h"

namespace {
class MatchFoundException: public std::exception {};
}

SongFilter
MakeSongFilter(const char *filter_string)
{
	auto vars = std::array<const char *, 1>{filter_string};
	auto args = std::span{vars};

	auto filter = SongFilter();

	filter.Parse(args, false);

	filter.Optimize();

	return filter;
}

SongFilter
MakeSongFilter(TagType tag_type, const char *tag_value)
{
	if (!sticker_allowed_tags[tag_type])
		throw std::runtime_error("tag type not allowed for sticker");

	auto vars = std::array<const char *, 2>{tag_item_names[tag_type], tag_value};
	auto args = std::span{vars};

	auto filter = SongFilter();

	filter.Parse(args, false);

	filter.Optimize();

	return filter;
}

SongFilter
MakeSongFilter(const std::string &sticker_type, const std::string &sticker_uri)
{
	if (sticker_type == "filter")
		return MakeSongFilter(sticker_uri.c_str());

	if (auto tag_type = tag_name_parse_i(sticker_type); tag_type != TAG_NUM_OF_ITEM_TYPES)
		return MakeSongFilter(tag_type, sticker_uri.c_str());

	return {};
}

SongFilter
MakeSongFilterNoThrow(const std::string &sticker_type, const std::string &sticker_uri) noexcept
{
	try {
		return MakeSongFilter(sticker_type, sticker_uri);
	}
	catch (std::exception& e) {
		// ignore
	}
	return {};
}

bool
TagExists(const Database &database, TagType tag_type, const char* tag_value)
{
	return FilterMatches(database, MakeSongFilter(tag_type, tag_value));
}

bool
FilterMatches(const Database &database, const SongFilter& filter) noexcept
{
	if (filter.IsEmpty())
		return false;

	const auto selection = DatabaseSelection{"", true, &filter};

	// TODO: we just need to know if the tag selection has a match.
	//       a visitor callback that can stop the db visit by return value
	//       may be cleaner than throwing an exception.
	try {
		database.Visit(selection, [](const LightSong &) {
			throw MatchFoundException{};
		});
	}
	catch (MatchFoundException& found) {
		return true;
	}

	return false;
}
