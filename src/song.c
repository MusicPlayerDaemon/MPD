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
#include "song.h"
#include "uri.h"
#include "directory.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>

static struct song *
song_alloc(const char *uri, struct directory *parent)
{
	size_t uri_length;
	struct song *song;

	assert(uri);
	uri_length = strlen(uri);
	assert(uri_length);
	song = g_malloc(sizeof(*song) - sizeof(song->uri) + uri_length + 1);

	song->tag = NULL;
	memcpy(song->uri, uri, uri_length + 1);
	song->parent = parent;
	song->mtime = 0;

	return song;
}

struct song *
song_remote_new(const char *uri)
{
	return song_alloc(uri, NULL);
}

struct song *
song_file_new(const char *path, struct directory *parent)
{
	assert((parent == NULL) == (*path == '/'));

	return song_alloc(path, parent);
}

void
song_free(struct song *song)
{
	if (song->tag)
		tag_free(song->tag);
	g_free(song);
}

char *
song_get_uri(const struct song *song)
{
	assert(song != NULL);
	assert(*song->uri);

	if (!song_in_database(song) || directory_is_root(song->parent))
		return g_strdup(song->uri);
	else
		return g_strconcat(directory_get_path(song->parent),
				   "/", song->uri, NULL);
}
