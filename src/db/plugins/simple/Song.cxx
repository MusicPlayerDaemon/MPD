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
#include "fs/Traits.hxx"

Song::Song(DetachedSong &&other, Directory &_parent) noexcept
	:tag(std::move(other.WritableTag())),
	 parent(_parent),
	 mtime(other.GetLastModified()),
	 start_time(other.GetStartTime()),
	 end_time(other.GetEndTime()),
	 filename(other.GetURI())
{
}

std::string
Song::GetURI() const noexcept
{
	if (parent.IsRoot())
		return filename;
	else {
		const char *path = parent.GetPath();
		return PathTraitsUTF8::Build(path, filename.c_str());
	}
}

LightSong
Song::Export() const noexcept
{
	LightSong dest(filename.c_str(), tag);
	if (!parent.IsRoot())
		dest.directory = parent.GetPath();
	if (!target.empty())
		dest.real_uri = target.c_str();
	dest.mtime = mtime;
	dest.start_time = start_time;
	dest.end_time = end_time;
	dest.audio_format = audio_format;
	return dest;
}
