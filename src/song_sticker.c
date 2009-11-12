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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "song_sticker.h"
#include "song.h"
#include "directory.h"
#include "sticker.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

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

bool
sticker_song_delete_value(const struct song *song, const char *name)
{
	char *uri;
	bool success;

	assert(song != NULL);
	assert(song_in_database(song));

	uri = song_get_uri(song);
	success = sticker_delete_value("song", uri, name);
	g_free(uri);

	return success;
}

struct sticker *
sticker_song_get(const struct song *song)
{
	char *uri;
	struct sticker *sticker;

	assert(song != NULL);
	assert(song_in_database(song));

	uri = song_get_uri(song);
	sticker = sticker_load("song", uri);
	g_free(uri);

	return sticker;
}

struct sticker_song_find_data {
	struct directory *directory;
	const char *base_uri;
	size_t base_uri_length;

	void (*func)(struct song *song, const char *value,
		     gpointer user_data);
	gpointer user_data;
};

static void
sticker_song_find_cb(const char *uri, const char *value, gpointer user_data)
{
	struct sticker_song_find_data *data = user_data;
	struct song *song;

	if (memcmp(uri, data->base_uri, data->base_uri_length) != 0)
		/* should not happen, ignore silently */
		return;

	song = directory_lookup_song(data->directory,
				     uri + data->base_uri_length);
	if (song != NULL)
		data->func(song, value, data->user_data);
}

bool
sticker_song_find(struct directory *directory, const char *name,
		  void (*func)(struct song *song, const char *value,
			       gpointer user_data),
		  gpointer user_data)
{
	struct sticker_song_find_data data = {
		.directory = directory,
		.func = func,
		.user_data = user_data,
	};
	char *allocated;
	bool success;

	data.base_uri = directory_get_path(directory);
	if (*data.base_uri != 0)
		/* append slash to base_uri */
		data.base_uri = allocated =
			g_strconcat(data.base_uri, "/", NULL);
	else
		/* searching in root directory - no trailing slash */
		allocated = NULL;

	data.base_uri_length = strlen(data.base_uri);

	success = sticker_find("song", data.base_uri, name,
			       sticker_song_find_cb, &data);
	g_free(allocated);

	return success;
}
