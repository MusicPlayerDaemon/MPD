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
#include "DatabasePlaylist.hxx"

extern "C" {
#include "dbUtils.h"
#include "locate.h"
#include "db_selection.h"
#include "stored_playlist.h"
}

#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"

#include <functional>

static bool
AddSong(const char *playlist_path_utf8,
	song &song, GError **error_r)
{
	return spl_append_song(playlist_path_utf8, &song, error_r);
}

bool
addAllInToStoredPlaylist(const char *uri_utf8, const char *playlist_path_utf8,
			 GError **error_r)
{
	struct db_selection selection;
	db_selection_init(&selection, uri_utf8, true);

	using namespace std::placeholders;
	const auto f = std::bind(AddSong, playlist_path_utf8, _1, _2);
	return GetDatabase()->Visit(selection, f, error_r);
}

static bool
SearchAddSong(const char *playlist_path_utf8,
	      const struct locate_item_list *criteria,
	      song &song, GError **error_r)
{
	return !locate_song_search(&song, criteria) ||
		spl_append_song(playlist_path_utf8, &song, error_r);
}

bool
search_add_to_playlist(const char *uri, const char *playlist_path_utf8,
		       const struct locate_item_list *criteria,
		       GError **error_r)
{
	struct db_selection selection;
	db_selection_init(&selection, uri, true);

	struct locate_item_list *new_list
		= locate_item_list_casefold(criteria);

	using namespace std::placeholders;
	const auto f = std::bind(SearchAddSong, playlist_path_utf8,
				 new_list, _1, _2);
	bool success = GetDatabase()->Visit(selection, f, error_r);

	locate_item_list_free(new_list);

	return success;
}
