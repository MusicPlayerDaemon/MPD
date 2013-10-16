/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "SongSticker.hxx"
#include "StickerDatabase.hxx"
#include "Song.hxx"
#include "Directory.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

char *
sticker_song_get_value(const Song *song, const char *name)
{
	assert(song != NULL);
	assert(song->IsInDatabase());

	const auto uri = song->GetURI();
	return sticker_load_value("song", uri.c_str(), name);
}

bool
sticker_song_set_value(const Song *song,
		       const char *name, const char *value)
{
	assert(song != NULL);
	assert(song->IsInDatabase());

	const auto uri = song->GetURI();
	return sticker_store_value("song", uri.c_str(), name, value);
}

bool
sticker_song_delete(const Song *song)
{
	assert(song != NULL);
	assert(song->IsInDatabase());

	const auto uri = song->GetURI();
	return sticker_delete("song", uri.c_str());
}

bool
sticker_song_delete_value(const Song *song, const char *name)
{
	assert(song != NULL);
	assert(song->IsInDatabase());

	const auto uri = song->GetURI();
	return sticker_delete_value("song", uri.c_str(), name);
}

struct sticker *
sticker_song_get(const Song *song)
{
	assert(song != NULL);
	assert(song->IsInDatabase());

	const auto uri = song->GetURI();
	return sticker_load("song", uri.c_str());
}

struct sticker_song_find_data {
	Directory *directory;
	const char *base_uri;
	size_t base_uri_length;

	void (*func)(Song *song, const char *value,
		     void *user_data);
	void *user_data;
};

static void
sticker_song_find_cb(const char *uri, const char *value, void *user_data)
{
	struct sticker_song_find_data *data =
		(struct sticker_song_find_data *)user_data;

	if (memcmp(uri, data->base_uri, data->base_uri_length) != 0)
		/* should not happen, ignore silently */
		return;

	Song *song = data->directory->LookupSong(uri + data->base_uri_length);
	if (song != NULL)
		data->func(song, value, data->user_data);
}

bool
sticker_song_find(Directory *directory, const char *name,
		  void (*func)(Song *song, const char *value,
			       void *user_data),
		  void *user_data)
{
	struct sticker_song_find_data data;
	data.directory = directory;
	data.func = func;
	data.user_data = user_data;

	char *allocated;
	data.base_uri = directory->GetPath();
	if (*data.base_uri != 0)
		/* append slash to base_uri */
		data.base_uri = allocated =
			g_strconcat(data.base_uri, "/", NULL);
	else
		/* searching in root directory - no trailing slash */
		allocated = NULL;

	data.base_uri_length = strlen(data.base_uri);

	bool success = sticker_find("song", data.base_uri, name,
				    sticker_song_find_cb, &data);
	g_free(allocated);

	return success;
}
