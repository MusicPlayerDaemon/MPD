// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
