/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
			       const char *base)
		:LightDirectory(directory),
		 buffer(IsRoot()
			? std::string(base)
			: PathTraitsUTF8::Build(base, uri)) {
		uri = buffer.c_str();
	}
};

static void
PrefixVisitDirectory(const char *base, const VisitDirectory &visit_directory,
		     const LightDirectory &directory)
{
	visit_directory(PrefixedLightDirectory(directory, base));
}

static void
PrefixVisitSong(const char *base, const VisitSong &visit_song,
		const LightSong &song)
{
	visit_song(PrefixedLightSong(song, base));
}

static void
PrefixVisitPlaylist(const char *base, const VisitPlaylist &visit_playlist,
		    const PlaylistInfo &playlist,
		    const LightDirectory &directory)
{
	visit_playlist(playlist,
		       PrefixedLightDirectory(directory, base));
}

void
WalkMount(const char *base, const Database &db,
	  const char *uri,
	  const DatabaseSelection &old_selection,
	  const VisitDirectory &visit_directory, const VisitSong &visit_song,
	  const VisitPlaylist &visit_playlist)
{
	using namespace std::placeholders;

	VisitDirectory vd;
	if (visit_directory)
		vd = std::bind(PrefixVisitDirectory,
			       base, std::ref(visit_directory), _1);

	VisitSong vs;
	if (visit_song)
		vs = std::bind(PrefixVisitSong,
			       base, std::ref(visit_song), _1);

	VisitPlaylist vp;
	if (visit_playlist)
		vp = std::bind(PrefixVisitPlaylist,
			       base, std::ref(visit_playlist), _1, _2);

	DatabaseSelection selection(old_selection);
	selection.uri = uri;

	SongFilter prefix_filter;

	if (base != nullptr && selection.filter != nullptr) {
		/* if the SongFilter contains a LOCATE_TAG_BASE_TYPE
		   item, copy the SongFilter and drop the mount point
		   from the filter, because the mounted database
		   doesn't know its own location within MPD's VFS */
		prefix_filter = selection.filter->WithoutBasePrefix(base);
		selection.filter = &prefix_filter;
	}

	db.Visit(selection, vd, vs, vp);
}
