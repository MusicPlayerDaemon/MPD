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
#include "SongPrint.hxx"
#include "db/LightSong.hxx"
#include "storage/StorageInterface.hxx"
#include "DetachedSong.hxx"
#include "TimePrint.hxx"
#include "TagPrint.hxx"
#include "client/Client.hxx"
#include "fs/Traits.hxx"
#include "util/UriUtil.hxx"

#define SONG_FILE "file: "

static void
song_print_uri(Client &client, const char *uri, bool base)
{
	std::string allocated;

	if (base) {
		uri = PathTraitsUTF8::GetBase(uri);
	} else {
#ifdef ENABLE_DATABASE
		const Storage *storage = client.GetStorage();
		if (storage != nullptr) {
			const char *suffix = storage->MapToRelativeUTF8(uri);
			if (suffix != nullptr)
				uri = suffix;
		}
#endif

		allocated = uri_remove_auth(uri);
		if (!allocated.empty())
			uri = allocated.c_str();
	}

	client_printf(client, "%s%s\n", SONG_FILE, uri);
}

void
song_print_uri(Client &client, const LightSong &song, bool base)
{
	if (!base && song.directory != nullptr) {
		client_printf(client, "%s%s/%s\n", SONG_FILE,
			      song.directory, song.uri);
	} else
		song_print_uri(client, song.uri, base);
}

void
song_print_uri(Client &client, const DetachedSong &song, bool base)
{
	song_print_uri(client, song.GetURI(), base);
}

void
song_print_info(Client &client, const LightSong &song, bool base)
{
	song_print_uri(client, song, base);

	const unsigned start_ms = song.start_time.ToMS();
	const unsigned end_ms = song.end_time.ToMS();

	if (end_ms > 0)
		client_printf(client, "Range: %u.%03u-%u.%03u\n",
			      start_ms / 1000,
			      start_ms % 1000,
			      end_ms / 1000,
			      end_ms % 1000);
	else if (start_ms > 0)
		client_printf(client, "Range: %u.%03u-\n",
			      start_ms / 1000,
			      start_ms % 1000);

	if (song.mtime > 0)
		time_print(client, "Last-Modified", song.mtime);

	tag_print(client, *song.tag);
}

void
song_print_info(Client &client, const DetachedSong &song, bool base)
{
	song_print_uri(client, song, base);

	const unsigned start_ms = song.GetStartTime().ToMS();
	const unsigned end_ms = song.GetEndTime().ToMS();

	if (end_ms > 0)
		client_printf(client, "Range: %u.%03u-%u.%03u\n",
			      start_ms / 1000,
			      start_ms % 1000,
			      end_ms / 1000,
			      end_ms % 1000);
	else if (start_ms > 0)
		client_printf(client, "Range: %u.%03u-\n",
			      start_ms / 1000,
			      start_ms % 1000);

	if (song.GetLastModified() > 0)
		time_print(client, "Last-Modified", song.GetLastModified());

	tag_print_values(client, song.GetTag());

	const auto duration = song.GetDuration();
	if (!duration.IsNegative())
		client_printf(client, "Time: %u\n", duration.RoundS());
}
