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

#include "DatabasePrint.hxx"
#include "Selection.hxx"
#include "SongPrint.hxx"
#include "TimePrint.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "song/LightSong.hxx"
#include "tag/Tag.hxx"
#include "LightDirectory.hxx"
#include "PlaylistInfo.hxx"
#include "Interface.hxx"
#include "fs/Traits.hxx"
#include "time/ChronoUtil.hxx"
#include "util/RecursiveMap.hxx"

#include <fmt/format.h>

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
	r.Fmt(FMT_STRING("directory: {}\n"),
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
		r.Fmt(FMT_STRING("playlist: {}\n"),
		      ApplyBaseFlag(name_utf8, base));
	else
		r.Fmt(FMT_STRING("playlist: {}/{}\n"),
		      directory, name_utf8);
}

static void
print_playlist_in_directory(Response &r, bool base,
			    const LightDirectory *directory,
			    const char *name_utf8) noexcept
{
	if (base || directory == nullptr || directory->IsRoot())
		r.Fmt(FMT_STRING("playlist: {}\n"), name_utf8);
	else
		r.Fmt(FMT_STRING("playlist: {}/{}\n"),
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

void
db_selection_print(Response &r, Partition &partition,
		   const DatabaseSelection &selection,
		   bool full, bool base)
{
	const Database &db = partition.GetDatabaseOrThrow();

	const auto d = selection.filter == nullptr
		? [&,base](const auto &dir)
			{ return full ?
				PrintDirectoryFull(r, base, dir) :
				PrintDirectoryBrief(r, base, dir); }
		: VisitDirectory();

	VisitSong s = [&,base](const auto &song)
		{ return full ?
			PrintSongFull(r, base, song) :
			PrintSongBrief(r, base, song); };

	const auto p = selection.filter == nullptr
		? [&,base](const auto &playlist, const auto &dir)
			{ return full ?
				PrintPlaylistFull(r, base, playlist, dir) :
				PrintPlaylistBrief(r, base, playlist, dir); }
		: VisitPlaylist();

	db.Visit(selection, d, s, p);
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

	const auto f = [&](const auto &song)
		{ return PrintSongURIVisitor(r, song); };

	db.Visit(selection, f);
}

static void
PrintUniqueTags(Response &r, ConstBuffer<TagType> tag_types,
		const RecursiveMap<std::string> &map) noexcept
{
	const char *const name = tag_item_names[tag_types.front()];
	tag_types.pop_front();

	for (const auto &[key, tag] : map) {
		r.Fmt(FMT_STRING("{}: {}\n"), name, key);

		if (!tag_types.empty())
			PrintUniqueTags(r, tag_types, tag);
	}
}

void
PrintUniqueTags(Response &r, Partition &partition,
		ConstBuffer<TagType> tag_types,
		const SongFilter *filter)
{
	const Database &db = partition.GetDatabaseOrThrow();

	const DatabaseSelection selection("", true, filter);

	PrintUniqueTags(r, tag_types,
			db.CollectUniqueTags(selection, tag_types));
}
