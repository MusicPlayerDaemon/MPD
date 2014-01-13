/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "util/Alloc.hxx"
#include "DetachedSong.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static Song *
song_alloc(const char *uri, Directory *parent)
{
	size_t uri_length;

	assert(uri);
	uri_length = strlen(uri);
	assert(uri_length);

	Song *song = (Song *)
		xalloc(sizeof(*song) - sizeof(song->uri) + uri_length + 1);

	song->tag = nullptr;
	memcpy(song->uri, uri, uri_length + 1);
	song->parent = parent;
	song->mtime = 0;
	song->start_ms = song->end_ms = 0;

	return song;
}

Song *
Song::NewFrom(DetachedSong &&other, Directory *parent)
{
	Song *song = song_alloc(other.GetURI(), parent);
	song->tag = new Tag(std::move(other.WritableTag()));
	song->mtime = other.GetLastModified();
	song->start_ms = other.GetStartMS();
	song->end_ms = other.GetEndMS();
	return song;
}

Song *
Song::NewFile(const char *path, Directory *parent)
{
	return song_alloc(path, parent);
}

void
Song::Free()
{
	delete tag;
	free(this);
}

std::string
Song::GetURI() const
{
	assert(*uri);

	if (parent == nullptr || parent->IsRoot())
		return std::string(uri);
	else {
		const char *path = parent->GetPath();

		std::string result;
		result.reserve(strlen(path) + 1 + strlen(uri));
		result.assign(path);
		result.push_back('/');
		result.append(uri);
		return result;
	}
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
