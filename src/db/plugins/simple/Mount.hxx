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
