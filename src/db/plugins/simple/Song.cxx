/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "Song.hxx"
#include "Directory.hxx"
#include "tag/Tag.hxx"
#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"
#include "util/StringView.hxx"

inline
Song::Song(StringView _uri, Directory &_parent) noexcept
	:parent(&_parent), uri(_uri.data, _uri.size)
{
}

static SongPtr
song_alloc(StringView uri, Directory &parent) noexcept
{
	return std::make_unique<Song>(uri, parent);
}

SongPtr
Song::NewFrom(DetachedSong &&other, Directory &parent) noexcept
{
	SongPtr song(song_alloc(other.GetURI(), parent));
	song->tag = std::move(other.WritableTag());
	song->mtime = other.GetLastModified();
	song->start_time = other.GetStartTime();
	song->end_time = other.GetEndTime();
	return song;
}

SongPtr
Song::NewFile(const char *path, Directory &parent) noexcept
{
	return SongPtr(song_alloc(path, parent));
}

std::string
Song::GetURI() const noexcept
{
	if (parent->IsRoot())
		return uri;
	else {
		const char *path = parent->GetPath();

		std::string result;
		result.reserve(strlen(path) + 1 + uri.length());
		result.assign(path);
		result.push_back('/');
		result.append(uri);
		return result;
	}
}

LightSong
Song::Export() const noexcept
{
	LightSong dest(uri.c_str(), tag);
	if (!parent->IsRoot())
		dest.directory = parent->GetPath();
	dest.mtime = mtime;
	dest.start_time = start_time;
	dest.end_time = end_time;
	dest.audio_format = audio_format;
	return dest;
}
