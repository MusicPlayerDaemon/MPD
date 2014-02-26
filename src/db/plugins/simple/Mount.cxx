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

#include "config.h"
#include "Mount.hxx"
#include "PrefixedLightSong.hxx"
#include "db/Selection.hxx"
#include "db/LightDirectory.hxx"
#include "db/LightSong.hxx"
#include "db/Interface.hxx"
#include "fs/Traits.hxx"
#include "util/Error.hxx"

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

static bool
PrefixVisitDirectory(const char *base, const VisitDirectory &visit_directory,
		     const LightDirectory &directory, Error &error)
{
	return visit_directory(PrefixedLightDirectory(directory, base), error);
}

static bool
PrefixVisitSong(const char *base, const VisitSong &visit_song,
		const LightSong &song, Error &error)
{
	return visit_song(PrefixedLightSong(song, base), error);
}

static bool
PrefixVisitPlaylist(const char *base, const VisitPlaylist &visit_playlist,
		    const PlaylistInfo &playlist,
		    const LightDirectory &directory,
		    Error &error)
{
	return visit_playlist(playlist,
			      PrefixedLightDirectory(directory, base),
			      error);
}

bool
WalkMount(const char *base, const Database &db,
	  bool recursive, const SongFilter *filter,
	  const VisitDirectory &visit_directory, const VisitSong &visit_song,
	  const VisitPlaylist &visit_playlist,
	  Error &error)
{
	using namespace std::placeholders;

	VisitDirectory vd;
	if (visit_directory)
		vd = std::bind(PrefixVisitDirectory,
			       base, std::ref(visit_directory), _1, _2);

	VisitSong vs;
	if (visit_song)
		vs = std::bind(PrefixVisitSong,
			       base, std::ref(visit_song), _1, _2);

	VisitPlaylist vp;
	if (visit_playlist)
		vp = std::bind(PrefixVisitPlaylist,
			       base, std::ref(visit_playlist), _1, _2, _3);

	return db.Visit(DatabaseSelection("", recursive, filter),
			vd, vs, vp, error);
}
