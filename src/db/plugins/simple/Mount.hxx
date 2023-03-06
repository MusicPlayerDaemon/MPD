// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DB_SIMPLE_MOUNT_HXX
#define MPD_DB_SIMPLE_MOUNT_HXX

#include "db/Visitor.hxx"

#include <string_view>

class Database;
struct DatabaseSelection;

void
WalkMount(std::string_view base, const Database &db,
	  std::string_view uri,
	  const DatabaseSelection &old_selection,
	  const VisitDirectory &visit_directory, const VisitSong &visit_song,
	  const VisitPlaylist &visit_playlist);

#endif
