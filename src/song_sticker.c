/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "song_sticker.h"
#include "song.h"
#include "sticker.h"

#include <glib.h>

#include <assert.h>

char *
sticker_song_get_value(const struct song *song, const char *name)
{
	char *uri, *value;

	assert(song != NULL);
	assert(song_in_database(song));

	uri = song_get_uri(song);
	value = sticker_load_value("song", uri, name);
	g_free(uri);

	return value;
}

bool
sticker_song_set_value(const struct song *song,
		       const char *name, const char *value)
{
	char *uri;
	bool ret;

	assert(song != NULL);
	assert(song_in_database(song));

	uri = song_get_uri(song);
	ret = sticker_store_value("song", uri, name, value);
	g_free(uri);

	return ret;
}

bool
sticker_song_delete(const struct song *song)
{
	char *uri;
	bool ret;

	assert(song != NULL);
	assert(song_in_database(song));

	uri = song_get_uri(song);
	ret = sticker_delete("song", uri);
	g_free(uri);

	return ret;
}
