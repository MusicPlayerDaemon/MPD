/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "SongFilter.hxx"
#include "SongPrint.hxx"
#include "TimePrint.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "tag/Tag.hxx"
#include "LightSong.hxx"
#include "LightDirectory.hxx"
#include "PlaylistInfo.hxx"
#include "Interface.hxx"
#include "fs/Traits.hxx"

#include <functional>

static const char *
ApplyBaseFlag(const char *uri, bool base)
{
	if (base)
		uri = PathTraitsUTF8::GetBase(uri);
	return uri;
}

static void
PrintDirectoryURI(Response &r, bool base, const LightDirectory &directory)
{
	r.Format("directory: %s\n",
		 ApplyBaseFlag(directory.GetPath(), base));
}

static bool
PrintDirectoryBrief(Response &r, bool base, const LightDirectory &directory)
{
	if (!directory.IsRoot())
		PrintDirectoryURI(r, base, directory);

	return true;
}

static bool
PrintDirectoryFull(Response &r, bool base, const LightDirectory &directory)
{
	if (!directory.IsRoot()) {
		PrintDirectoryURI(r, base, directory);

		if (directory.mtime > 0)
			time_print(r, "Last-Modified", directory.mtime);
	}

	return true;
}

static void
print_playlist_in_directory(Response &r, bool base,
			    const char *directory,
			    const char *name_utf8)
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
			    const char *name_utf8)
{
	if (base || directory == nullptr || directory->IsRoot())
		r.Format("playlist: %s\n", name_utf8);
	else
		r.Format("playlist: %s/%s\n",
			 directory->GetPath(), name_utf8);
}

static bool
PrintSongBrief(Response &r, Partition &partition,
	       bool base, const LightSong &song)
{
	song_print_uri(r, partition, song, base);

	if (song.tag->has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(r, base,
					    song.directory, song.uri);

	return true;
}

static bool
PrintSongFull(Response &r, Partition &partition,
	      bool base, const LightSong &song)
{
	song_print_info(r, partition, song, base);

	if (song.tag->has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(r, base,
					    song.directory, song.uri);

	return true;
}

static bool
PrintPlaylistBrief(Response &r, bool base,
		   const PlaylistInfo &playlist,
		   const LightDirectory &directory)
{
	print_playlist_in_directory(r, base,
				    &directory, playlist.name.c_str());
	return true;
}

static bool
PrintPlaylistFull(Response &r, bool base,
		  const PlaylistInfo &playlist,
		  const LightDirectory &directory)
{
	print_playlist_in_directory(r, base,
				    &directory, playlist.name.c_str());

	if (playlist.mtime > 0)
		time_print(r, "Last-Modified", playlist.mtime);

	return true;
}

bool
db_selection_print(Response &r, Partition &partition,
		   const DatabaseSelection &selection,
		   bool full, bool base,
		   unsigned window_start, unsigned window_end,
		   Error &error)
{
	const Database *db = partition.GetDatabase(error);
	if (db == nullptr)
		return false;

	unsigned i = 0;

	using namespace std::placeholders;
	const auto d = selection.filter == nullptr
		? std::bind(full ? PrintDirectoryFull : PrintDirectoryBrief,
			    std::ref(r), base, _1)
		: VisitDirectory();
	VisitSong s = std::bind(full ? PrintSongFull : PrintSongBrief,
				std::ref(r), std::ref(partition), base, _1);
	const auto p = selection.filter == nullptr
		? std::bind(full ? PrintPlaylistFull : PrintPlaylistBrief,
			    std::ref(r), base, _1, _2)
		: VisitPlaylist();

	if (window_start > 0 ||
	    window_end < (unsigned)std::numeric_limits<int>::max())
		s = [s, window_start, window_end, &i](const LightSong &song,
						      Error &error2){
			const bool in_window = i >= window_start && i < window_end;
			++i;
			return !in_window || s(song, error2);
		};

	return db->Visit(selection, d, s, p, error);
}

bool
db_selection_print(Response &r, Partition &partition,
		   const DatabaseSelection &selection,
		   bool full, bool base,
		   Error &error)
{
	return db_selection_print(r, partition, selection, full, base,
				  0, std::numeric_limits<int>::max(),
				  error);
}

static bool
PrintSongURIVisitor(Response &r, Partition &partition, const LightSong &song)
{
	song_print_uri(r, partition, song);

	return true;
}

static bool
PrintUniqueTag(Response &r, TagType tag_type,
	       const Tag &tag)
{
	const char *value = tag.GetValue(tag_type);
	assert(value != nullptr);
	r.Format("%s: %s\n", tag_item_names[tag_type], value);

	for (const auto &item : tag)
		if (item.type != tag_type)
			r.Format("%s: %s\n",
				 tag_item_names[item.type], item.value);

	return true;
}

bool
PrintUniqueTags(Response &r, Partition &partition,
		unsigned type, tag_mask_t group_mask,
		const SongFilter *filter,
		Error &error)
{
	const Database *db = partition.GetDatabase(error);
	if (db == nullptr)
		return false;

	const DatabaseSelection selection("", true, filter);

	if (type == LOCATE_TAG_FILE_TYPE) {
		using namespace std::placeholders;
		const auto f = std::bind(PrintSongURIVisitor,
					 std::ref(r), std::ref(partition), _1);
		return db->Visit(selection, f, error);
	} else {
		assert(type < TAG_NUM_OF_ITEM_TYPES);

		using namespace std::placeholders;
		const auto f = std::bind(PrintUniqueTag, std::ref(r),
					 (TagType)type, _1);
		return db->VisitUniqueTags(selection, (TagType)type,
					   group_mask,
					   f, error);
	}
}
