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
#include "Song.hxx"
#include "Directory.hxx"
#include "tag/Tag.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

Directory detached_root;

static Song *
song_alloc(const char *uri, Directory *parent)
{
	size_t uri_length;

	assert(uri);
	uri_length = strlen(uri);
	assert(uri_length);

	Song *song = (Song *)
		g_malloc(sizeof(*song) - sizeof(song->uri) + uri_length + 1);

	song->tag = nullptr;
	memcpy(song->uri, uri, uri_length + 1);
	song->parent = parent;
	song->mtime = 0;
	song->start_ms = song->end_ms = 0;

	return song;
}

Song *
Song::NewRemote(const char *uri)
{
	return song_alloc(uri, nullptr);
}

Song *
Song::NewFile(const char *path, Directory *parent)
{
	assert((parent == nullptr) == (*path == '/'));

	return song_alloc(path, parent);
}

Song *
Song::ReplaceURI(const char *new_uri)
{
	Song *new_song = song_alloc(new_uri, parent);
	new_song->tag = tag;
	new_song->mtime = mtime;
	new_song->start_ms = start_ms;
	new_song->end_ms = end_ms;
	g_free(this);
	return new_song;
}

Song *
Song::NewDetached(const char *uri)
{
	assert(uri != nullptr);

	return song_alloc(uri, &detached_root);
}

Song *
Song::DupDetached() const
{
	Song *song;
	if (IsInDatabase()) {
		char *new_uri = GetURI();
		song = NewDetached(new_uri);
		g_free(new_uri);
	} else
		song = song_alloc(uri, nullptr);

	song->tag = tag != nullptr ? new Tag(*tag) : nullptr;
	song->mtime = mtime;
	song->start_ms = start_ms;
	song->end_ms = end_ms;

	return song;
}

void
Song::Free()
{
	delete tag;
	g_free(this);
}

gcc_pure
static inline bool
directory_equals(const Directory &a, const Directory &b)
{
	return strcmp(a.path, b.path) == 0;
}

gcc_pure
static inline bool
directory_is_same(const Directory *a, const Directory *b)
{
	return a == b ||
		(a != nullptr && b != nullptr &&
		 directory_equals(*a, *b));

}

bool
song_equals(const Song *a, const Song *b)
{
	assert(a != nullptr);
	assert(b != nullptr);

	if (a->parent != nullptr && b->parent != nullptr &&
	    !directory_equals(*a->parent, *b->parent) &&
	    (a->parent == &detached_root || b->parent == &detached_root)) {
		/* must compare the full URI if one of the objects is
		   "detached" */
		char *au = a->GetURI();
		char *bu = b->GetURI();
		const bool result = strcmp(au, bu) == 0;
		g_free(bu);
		g_free(au);
		return result;
	}

	return directory_is_same(a->parent, b->parent) &&
		strcmp(a->uri, b->uri) == 0;
}

char *
Song::GetURI() const
{
	assert(*uri);

	if (!IsInDatabase() || parent->IsRoot())
		return g_strdup(uri);
	else
		return g_strconcat(parent->GetPath(),
				   "/", uri, nullptr);
}

double
Song::GetDuration() const
{
	if (end_ms > 0)
		return (end_ms - start_ms) / 1000.0;

	if (tag == nullptr)
		return 0;

	return tag->time - start_ms / 1000.0;
}
