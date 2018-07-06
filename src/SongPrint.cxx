/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "SongPrint.hxx"
#include "db/LightSong.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "storage/StorageInterface.hxx"
#include "DetachedSong.hxx"
#include "TimePrint.hxx"
#include "TagPrint.hxx"
#include "client/Response.hxx"
#include "fs/Traits.hxx"
#include "util/ChronoUtil.hxx"
#include "util/UriUtil.hxx"

#define SONG_FILE "file: "

static void
song_print_uri(Response &r, const char *uri, bool base) noexcept
{
	std::string allocated;

	if (base) {
		uri = PathTraitsUTF8::GetBase(uri);
	} else {
		allocated = uri_remove_auth(uri);
		if (!allocated.empty())
			uri = allocated.c_str();
	}

	r.Format(SONG_FILE "%s\n", uri);
}

void
song_print_uri(Response &r, const LightSong &song, bool base) noexcept
{
	if (!base && song.directory != nullptr)
		r.Format(SONG_FILE "%s/%s\n", song.directory, song.uri);
	else
		song_print_uri(r, song.uri, base);
}

void
song_print_uri(Response &r, const DetachedSong &song, bool base) noexcept
{
	song_print_uri(r, song.GetURI(), base);
}

static void
PrintRange(Response &r, SongTime start_time, SongTime end_time) noexcept
{
	const unsigned start_ms = start_time.ToMS();
	const unsigned end_ms = end_time.ToMS();

	if (end_ms > 0)
		r.Format("Range: %u.%03u-%u.%03u\n",
			 start_ms / 1000,
			 start_ms % 1000,
			 end_ms / 1000,
			 end_ms % 1000);
	else if (start_ms > 0)
		r.Format("Range: %u.%03u-\n",
			 start_ms / 1000,
			 start_ms % 1000);
}

void
song_print_info(Response &r, const LightSong &song, bool base) noexcept
{
	song_print_uri(r, song, base);

	PrintRange(r, song.start_time, song.end_time);

	if (!IsNegative(song.mtime))
		time_print(r, "Last-Modified", song.mtime);

	tag_print(r, song.tag);
}

void
song_print_info(Response &r, const DetachedSong &song, bool base) noexcept
{
	song_print_uri(r, song, base);

	PrintRange(r, song.GetStartTime(), song.GetEndTime());

	if (!IsNegative(song.GetLastModified()))
		time_print(r, "Last-Modified", song.GetLastModified());

	tag_print_values(r, song.GetTag());

	const auto duration = song.GetDuration();
	if (!duration.IsNegative())
		r.Format("Time: %i\n"
			 "duration: %1.3f\n",
			 duration.RoundS(),
			 duration.ToDoubleS());
}
