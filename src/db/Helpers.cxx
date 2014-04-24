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

#include "Helpers.hxx"
#include "Stats.hxx"
#include "Interface.hxx"
#include "LightSong.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/Set.hxx"

#include <functional>
#include <set>

#include <assert.h>
#include <string.h>

struct StringLess {
	gcc_pure
	bool operator()(const char *a, const char *b) const {
		return strcmp(a, b) < 0;
	}
};

typedef std::set<const char *, StringLess> StringSet;

/**
 * Copy all tag items of the specified type.
 */
static bool
CopyTagItem(TagBuilder &dest, TagType dest_type,
	    const Tag &src, TagType src_type)
{
	bool found = false;
	const unsigned n = src.num_items;
	for (unsigned i = 0; i < n; ++i) {
		if (src.items[i]->type == src_type) {
			dest.AddItem(dest_type, src.items[i]->value);
			found = true;
		}
	}

	return found;
}

/**
 * Copy all tag items of the specified type.  Fall back to "Artist" if
 * there is no "AlbumArtist".
 */
static void
CopyTagItem(TagBuilder &dest, const Tag &src, TagType type)
{
	if (!CopyTagItem(dest, type, src, type) &&
	    type == TAG_ALBUM_ARTIST)
		CopyTagItem(dest, type, src, TAG_ARTIST);
}

/**
 * Copy all tag items of the types in the mask.
 */
static void
CopyTagMask(TagBuilder &dest, const Tag &src, uint32_t mask)
{
	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if ((mask & (1u << i)) != 0)
			CopyTagItem(dest, src, TagType(i));
}

static void
InsertUniqueTag(TagSet &set, const Tag &src, TagType type, const char *value,
		uint32_t group_mask)
{
	TagBuilder builder;
	if (value == nullptr)
		builder.AddEmptyItem(type);
	else
		builder.AddItem(type, value);
	CopyTagMask(builder, src, group_mask);
#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
	set.emplace(builder.Commit());
#else
	set.insert(builder.Commit());
#endif
}

static bool
CheckUniqueTag(TagSet &set, TagType dest_type,
	       const Tag &tag, TagType src_type,
	       uint32_t group_mask)
{
	bool found = false;
	for (unsigned i = 0; i < tag.num_items; ++i) {
		if (tag.items[i]->type == src_type) {
			InsertUniqueTag(set, tag, dest_type,
					tag.items[i]->value,
					group_mask);
			found = true;
		}
	}

	return found;
}

static bool
CollectTags(TagSet &set, TagType tag_type, uint32_t group_mask,
	    const LightSong &song)
{
	static_assert(sizeof(group_mask) * 8 >= TAG_NUM_OF_ITEM_TYPES,
		      "Mask is too small");

	assert((group_mask & (1u << unsigned(tag_type))) == 0);

	assert(song.tag != nullptr);
	const Tag &tag = *song.tag;

	if (!CheckUniqueTag(set, tag_type, tag, tag_type, group_mask) &&
	    (tag_type != TAG_ALBUM_ARTIST ||
	     /* fall back to "Artist" if no "AlbumArtist" was found */
	     !CheckUniqueTag(set, tag_type, tag, TAG_ARTIST, group_mask)))
		InsertUniqueTag(set, tag, tag_type, nullptr, group_mask);

	return true;
}

bool
VisitUniqueTags(const Database &db, const DatabaseSelection &selection,
		TagType tag_type, uint32_t group_mask,
		VisitTag visit_tag,
		Error &error)
{
	TagSet set;

	using namespace std::placeholders;
	const auto f = std::bind(CollectTags, std::ref(set),
				 tag_type, group_mask, _1);
	if (!db.Visit(selection, f, error))
		return false;

	for (const auto &value : set)
		if (!visit_tag(value, error))
			return false;

	return true;
}

static void
StatsVisitTag(DatabaseStats &stats, StringSet &artists, StringSet &albums,
	      const Tag &tag)
{
	if (tag.time > 0)
		stats.total_duration += tag.time;

	for (unsigned i = 0; i < tag.num_items; ++i) {
		const TagItem &item = *tag.items[i];

		switch (item.type) {
		case TAG_ARTIST:
#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
			artists.emplace(item.value);
#else
			artists.insert(item.value);
#endif
			break;

		case TAG_ALBUM:
#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
			albums.emplace(item.value);
#else
			albums.insert(item.value);
#endif
			break;

		default:
			break;
		}
	}
}

static bool
StatsVisitSong(DatabaseStats &stats, StringSet &artists, StringSet &albums,
	       const LightSong &song)
{
	++stats.song_count;

	StatsVisitTag(stats, artists, albums, *song.tag);

	return true;
}

bool
GetStats(const Database &db, const DatabaseSelection &selection,
	 DatabaseStats &stats, Error &error)
{
	stats.Clear();

	StringSet artists, albums;
	using namespace std::placeholders;
	const auto f = std::bind(StatsVisitSong,
				 std::ref(stats), std::ref(artists),
				 std::ref(albums), _1);
	if (!db.Visit(selection, f, error))
		return false;

	stats.artist_count = artists.size();
	stats.album_count = albums.size();
	return true;
}
