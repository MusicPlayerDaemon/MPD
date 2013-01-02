/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "Directory.hxx"

extern "C" {
#include "tag.h"
}

#include <glib.h>

#include <assert.h>

struct directory detached_root;

static struct song *
song_alloc(const char *uri, struct directory *parent)
{
	size_t uri_length;

	assert(uri);
	uri_length = strlen(uri);
	assert(uri_length);

	struct song *song = (struct song *)
		g_malloc(sizeof(*song) - sizeof(song->uri) + uri_length + 1);

	song->tag = nullptr;
	memcpy(song->uri, uri, uri_length + 1);
	song->parent = parent;
	song->mtime = 0;
	song->start_ms = song->end_ms = 0;

	return song;
}

struct song *
song_remote_new(const char *uri)
{
	return song_alloc(uri, nullptr);
}

struct song *
song_file_new(const char *path, struct directory *parent)
{
	assert((parent == nullptr) == (*path == '/'));

	return song_alloc(path, parent);
}

struct song *
song_replace_uri(struct song *old_song, const char *uri)
{
	struct song *new_song = song_alloc(uri, old_song->parent);
	new_song->tag = old_song->tag;
	new_song->mtime = old_song->mtime;
	new_song->start_ms = old_song->start_ms;
	new_song->end_ms = old_song->end_ms;
	g_free(old_song);
	return new_song;
}

struct song *
song_detached_new(const char *uri)
{
	assert(uri != nullptr);

	return song_alloc(uri, &detached_root);
}

struct song *
song_dup_detached(const struct song *src)
{
	assert(src != nullptr);

	struct song *song;
	if (song_in_database(src)) {
		char *uri = song_get_uri(src);
		song = song_detached_new(uri);
		g_free(uri);
	} else
		song = song_alloc(src->uri, nullptr);

	song->tag = tag_dup(src->tag);
	song->mtime = src->mtime;
	song->start_ms = src->start_ms;
	song->end_ms = src->end_ms;

	return song;
}

void
song_free(struct song *song)
{
	if (song->tag)
		tag_free(song->tag);
	g_free(song);
}

gcc_pure
static inline bool
directory_equals(const struct directory &a, const struct directory &b)
{
	return strcmp(a.path, b.path) == 0;
}

gcc_pure
static inline bool
directory_is_same(const struct directory *a, const struct directory *b)
{
	return a == b ||
		(a != nullptr && b != nullptr &&
		 directory_equals(*a, *b));

}

bool
song_equals(const struct song *a, const struct song *b)
{
	assert(a != nullptr);
	assert(b != nullptr);

	if (a->parent != nullptr && b->parent != nullptr &&
	    !directory_equals(*a->parent, *b->parent) &&
	    (a->parent == &detached_root || b->parent == &detached_root)) {
		/* must compare the full URI if one of the objects is
		   "detached" */
		char *au = song_get_uri(a);
		char *bu = song_get_uri(b);
		const bool result = strcmp(au, bu) == 0;
		g_free(bu);
		g_free(au);
		return result;
	}

	return directory_is_same(a->parent, b->parent) &&
		strcmp(a->uri, b->uri) == 0;
}

char *
song_get_uri(const struct song *song)
{
	assert(song != nullptr);
	assert(*song->uri);

	if (!song_in_database(song) || directory_is_root(song->parent))
		return g_strdup(song->uri);
	else
		return g_strconcat(directory_get_path(song->parent),
				   "/", song->uri, nullptr);
}

double
song_get_duration(const struct song *song)
{
	if (song->end_ms > 0)
		return (song->end_ms - song->start_ms) / 1000.0;

	if (song->tag == nullptr)
		return 0;

	return song->tag->time - song->start_ms / 1000.0;
}
