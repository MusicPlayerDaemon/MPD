/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "DatabasePrint.hxx"
#include "Selection.hxx"
#include "SongPrint.hxx"
#include "TimePrint.hxx"
#include "TagPrint.hxx"
#include "client/Response.hxx"
#include "protocol/RangeArg.hxx"
#include "Partition.hxx"
#include "song/DetachedSong.hxx"
#include "song/Filter.hxx"
#include "song/LightSong.hxx"
#include "tag/Tag.hxx"
#include "tag/Mask.hxx"
#include "LightDirectory.hxx"
#include "PlaylistInfo.hxx"
#include "Interface.hxx"
#include "fs/Traits.hxx"
#include "util/ChronoUtil.hxx"

#include <functional>

gcc_pure
static const char *
ApplyBaseFlag(const char *uri, bool base) noexcept
{
	if (base)
		uri = PathTraitsUTF8::GetBase(uri);
	return uri;
}

static void
PrintDirectoryURI(Response &r, bool base,
		  const LightDirectory &directory) noexcept
{
	r.Format("directory: %s\n",
		 ApplyBaseFlag(directory.GetPath(), base));
}

static void
PrintDirectoryBrief(Response &r, bool base,
		    const LightDirectory &directory) noexcept
{
	if (!directory.IsRoot())
		PrintDirectoryURI(r, base, directory);
}

static void
PrintDirectoryFull(Response &r, bool base,
		   const LightDirectory &directory) noexcept
{
	if (!directory.IsRoot()) {
		PrintDirectoryURI(r, base, directory);

		if (!IsNegative(directory.mtime))
			time_print(r, "Last-Modified", directory.mtime);
	}
}

static void
print_playlist_in_directory(Response &r, bool base,
			    const char *directory,
			    const char *name_utf8) noexcept
{
	if (base || directory == nullptr)
		r.Format("playlist: %s\n",
			 ApplyBaseFlag(name_utf8, base));
	else
		r.Format("playlist: %s/%s\n",
			 directory, name_utf8);
}

static void
print_playlist_in_directory(Response &r, bool base,
			    const LightDirectory *directory,
			    const char *name_utf8) noexcept
{
	if (base || directory == nullptr || directory->IsRoot())
		r.Format("playlist: %s\n", name_utf8);
	else
		r.Format("playlist: %s/%s\n",
			 directory->GetPath(), name_utf8);
}

static void
PrintSongBrief(Response &r, bool base, const LightSong &song) noexcept
{
	song_print_uri(r, song, base);

	if (song.tag.has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(r, base,
					    song.directory, song.uri);
}

static void
PrintSongFull(Response &r, bool base, const LightSong &song) noexcept
{
	song_print_info(r, song, base);

	if (song.tag.has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(r, base,
					    song.directory, song.uri);
}

static void
PrintPlaylistBrief(Response &r, bool base,
		   const PlaylistInfo &playlist,
		   const LightDirectory &directory) noexcept
{
	print_playlist_in_directory(r, base,
				    &directory, playlist.name.c_str());
}

static void
PrintPlaylistFull(Response &r, bool base,
		  const PlaylistInfo &playlist,
		  const LightDirectory &directory) noexcept
{
	print_playlist_in_directory(r, base,
				    &directory, playlist.name.c_str());

	if (!IsNegative(playlist.mtime))
		time_print(r, "Last-Modified", playlist.mtime);
}

gcc_pure
static bool
CompareNumeric(const char *a, const char *b) noexcept
{
	long a_value = strtol(a, nullptr, 10);
	long b_value = strtol(b, nullptr, 10);

	return a_value < b_value;
}

gcc_pure
static bool
CompareTags(TagType type, bool descending, const Tag &a, const Tag &b) noexcept
{
	const char *a_value = a.GetSortValue(type);
	const char *b_value = b.GetSortValue(type);

	if (descending) {
		using std::swap;
		swap(a_value, b_value);
	}

	switch (type) {
	case TAG_DISC:
	case TAG_TRACK:
		return CompareNumeric(a_value, b_value);

	default:
		return strcmp(a_value, b_value) < 0;
	}
}

void
db_selection_print(Response &r, Partition &partition,
		   const DatabaseSelection &selection,
		   bool full, bool base,
		   TagType sort, bool descending,
		   RangeArg window)
{
	const Database &db = partition.GetDatabaseOrThrow();

	unsigned i = 0;

	using namespace std::placeholders;
	const auto d = selection.filter == nullptr
		? std::bind(full ? PrintDirectoryFull : PrintDirectoryBrief,
			    std::ref(r), base, _1)
		: VisitDirectory();
	VisitSong s = std::bind(full ? PrintSongFull : PrintSongBrief,
				std::ref(r), base, _1);
	const auto p = selection.filter == nullptr
		? std::bind(full ? PrintPlaylistFull : PrintPlaylistBrief,
			    std::ref(r), base, _1, _2)
		: VisitPlaylist();

	if (sort == TAG_NUM_OF_ITEM_TYPES) {
		if (!window.IsAll())
			s = [s, window, &i](const LightSong &song){
				const bool in_window = i >= window.start && i < window.end;
				++i;
				if (in_window)
					s(song);
			};

		db.Visit(selection, d, s, p);
	} else {
		// TODO: allow the database plugin to sort internally

		/* the client has asked us to sort the result; this is
		   pretty expensive, because instead of streaming the
		   result to the client, we need to copy it all into
		   this std::vector, and then sort it */
		std::vector<DetachedSong> songs;

		{
			auto collect_songs = [&songs](const LightSong &song){
				songs.emplace_back(song);
			};

			db.Visit(selection, d, collect_songs, p);
		}

		if (sort == TagType(SORT_TAG_LAST_MODIFIED))
			std::stable_sort(songs.begin(), songs.end(),
					 [descending](const DetachedSong &a, const DetachedSong &b){
						 return descending
							 ? a.GetLastModified() > b.GetLastModified()
							 : a.GetLastModified() < b.GetLastModified();
					 });
		else
			std::stable_sort(songs.begin(), songs.end(),
					 [sort, descending](const DetachedSong &a,
							    const DetachedSong &b){
						 return CompareTags(sort, descending,
								    a.GetTag(),
								    b.GetTag());
					 });

		if (window.end < songs.size())
			songs.erase(std::next(songs.begin(), window.end),
				    songs.end());

		if (window.start >= songs.size())
			return;

		songs.erase(songs.begin(),
			    std::next(songs.begin(), window.start));

		for (const auto &song : songs)
			s((LightSong)song);
	}
}

void
db_selection_print(Response &r, Partition &partition,
		   const DatabaseSelection &selection,
		   bool full, bool base)
{
	db_selection_print(r, partition, selection, full, base,
			   TAG_NUM_OF_ITEM_TYPES, false,
			   RangeArg::All());
}

static void
PrintSongURIVisitor(Response &r, const LightSong &song) noexcept
{
	song_print_uri(r, song);
}

void
PrintSongUris(Response &r, Partition &partition,
	      const SongFilter *filter)
{
	const Database &db = partition.GetDatabaseOrThrow();

	const DatabaseSelection selection("", true, filter);

	using namespace std::placeholders;
	const auto f = std::bind(PrintSongURIVisitor,
				 std::ref(r), _1);
	db.Visit(selection, f);
}

static void
PrintUniqueTag(Response &r, TagType tag_type,
	       const Tag &tag) noexcept
{
	const char *value = tag.GetValue(tag_type);
	assert(value != nullptr);
	tag_print(r, tag_type, value);

	const auto tag_mask = r.GetTagMask();
	for (const auto &item : tag)
		if (item.type != tag_type && tag_mask.Test(item.type))
			tag_print(r, item.type, item.value);
}

void
PrintUniqueTags(Response &r, Partition &partition,
		TagType type, TagMask group_mask,
		const SongFilter *filter)
{
	assert(type < TAG_NUM_OF_ITEM_TYPES);

	const Database &db = partition.GetDatabaseOrThrow();

	const DatabaseSelection selection("", true, filter);

	using namespace std::placeholders;
	const auto f = std::bind(PrintUniqueTag, std::ref(r), type, _1);
	db.VisitUniqueTags(selection, type, group_mask, f);
}
