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

#include "Mount.hxx"
#include "PrefixedLightSong.hxx"
#include "song/Filter.hxx"
#include "db/Selection.hxx"
#include "db/LightDirectory.hxx"
#include "db/Interface.hxx"
#include "fs/Traits.hxx"

#include <string>

struct PrefixedLightDirectory : LightDirectory {
	std::string buffer;

	PrefixedLightDirectory(const LightDirectory &directory,
			       std::string_view base)
		:LightDirectory(directory),
		 buffer(IsRoot()
			? std::string(base)
			: PathTraitsUTF8::Build(base, uri)) {
		uri = buffer.c_str();
	}
};

static void
PrefixVisitDirectory(std::string_view base, const VisitDirectory &visit_directory,
		     const LightDirectory &directory)
{
	visit_directory(PrefixedLightDirectory(directory, base));
}

static void
PrefixVisitSong(std::string_view base, const VisitSong &visit_song,
		const LightSong &song)
{
	visit_song(PrefixedLightSong(song, base));
}

static void
PrefixVisitPlaylist(std::string_view base, const VisitPlaylist &visit_playlist,
		    const PlaylistInfo &playlist,
		    const LightDirectory &directory)
{
	visit_playlist(playlist,
		       PrefixedLightDirectory(directory, base));
}

void
WalkMount(std::string_view base, const Database &db,
	  std::string_view uri,
	  const DatabaseSelection &old_selection,
	  const VisitDirectory &visit_directory, const VisitSong &visit_song,
	  const VisitPlaylist &visit_playlist)
{
	VisitDirectory vd;
	if (visit_directory)
		vd = [base,&visit_directory](const auto &dir)
			{ return PrefixVisitDirectory(base, visit_directory, dir); };

	VisitSong vs;
	if (visit_song)
		vs = [base,&visit_song](const auto &song)
			{ return PrefixVisitSong(base, visit_song, song); };

	VisitPlaylist vp;
	if (visit_playlist)
		vp = [base,&visit_playlist](const auto &playlist, const auto &dir)
			{ return PrefixVisitPlaylist(base, visit_playlist, playlist, dir); };

	DatabaseSelection selection(old_selection);
	selection.uri = uri;

	SongFilter prefix_filter;

	if (base.data() != nullptr && selection.filter != nullptr) {
		/* if the SongFilter contains a LOCATE_TAG_BASE_TYPE
		   item, copy the SongFilter and drop the mount point
		   from the filter, because the mounted database
		   doesn't know its own location within MPD's VFS */
		prefix_filter = selection.filter->WithoutBasePrefix(base);
		selection.filter = &prefix_filter;
	}

	db.Visit(selection, vd, vs, vp);
}
