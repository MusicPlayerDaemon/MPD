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
#include "util/VarSize.hxx"
#include "DetachedSong.hxx"
#include "db/LightSong.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

inline Song::Song(const char *_uri, size_t uri_length, Directory &_parent)
	:parent(&_parent), mtime(0),
	 start_time(SongTime::zero()), end_time(SongTime::zero())
{
	memcpy(uri, _uri, uri_length + 1);
}

inline Song::~Song()
{
}

static Song *
song_alloc(const char *uri, Directory &parent)
{
	size_t uri_length;

	assert(uri);
	uri_length = strlen(uri);
	assert(uri_length);

	return NewVarSize<Song>(sizeof(Song::uri),
				uri_length + 1,
				uri, uri_length, parent);
}

Song *
Song::NewFrom(DetachedSong &&other, Directory &parent)
{
	Song *song = song_alloc(other.GetURI(), parent);
	song->tag = std::move(other.WritableTag());
	song->mtime = other.GetLastModified();
	song->start_time = other.GetStartTime();
	song->end_time = other.GetEndTime();
	return song;
}

Song *
Song::NewFile(const char *path, Directory &parent)
{
	return song_alloc(path, parent);
}

void
Song::Free()
{
	DeleteVarSize(this);
}

std::string
Song::GetURI() const
{
	assert(*uri);

	if (parent->IsRoot())
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

LightSong
Song::Export() const
{
	LightSong dest;
	dest.directory = parent->IsRoot()
		? nullptr : parent->GetPath();
	dest.uri = uri;
	dest.real_uri = nullptr;
	dest.tag = &tag;
	dest.mtime = mtime;
	dest.start_time = start_time;
	dest.end_time = end_time;
	return dest;
}
