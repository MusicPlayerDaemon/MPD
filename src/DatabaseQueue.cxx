/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "DatabaseQueue.hxx"
#include "DatabaseSelection.hxx"

extern "C" {
#include "playlist.h"
}

#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"

#include <functional>

static bool
AddToQueue(struct player_control *pc, song &song, GError **error_r)
{
	enum playlist_result result =
		playlist_append_song(&g_playlist, pc, &song, NULL);
	if (result != PLAYLIST_RESULT_SUCCESS) {
		g_set_error(error_r, playlist_quark(), result,
			    "Playlist error");
		return false;
	}

	return true;
}

bool
findAddIn(struct player_control *pc, const char *uri,
	  const struct locate_item_list *criteria, GError **error_r)
{
	const Database *db = GetDatabase(error_r);
	if (db == nullptr)
		return false;

	const DatabaseSelection selection(uri, true, criteria);

	using namespace std::placeholders;
	const auto f = std::bind(AddToQueue, pc, _1, _2);
	return db->Visit(selection, f, error_r);
}
