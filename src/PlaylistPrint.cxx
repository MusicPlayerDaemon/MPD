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
#include "PlaylistPrint.hxx"
#include "PlaylistFile.hxx"
#include "queue/Playlist.hxx"
#include "queue/QueuePrint.hxx"
#include "SongPrint.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "db/Interface.hxx"
#include "client/Client.hxx"
#include "input/InputStream.hxx"
#include "DetachedSong.hxx"
#include "fs/Traits.hxx"
#include "util/Error.hxx"
#include "thread/Cond.hxx"

#define SONG_FILE "file: "
#define SONG_TIME "Time: "

void
playlist_print_uris(Client &client, const playlist &playlist)
{
	const Queue &queue = playlist.queue;

	queue_print_uris(client, queue, 0, queue.GetLength());
}

bool
playlist_print_info(Client &client, const playlist &playlist,
		    unsigned start, unsigned end)
{
	const Queue &queue = playlist.queue;

	if (end > queue.GetLength())
		/* correct the "end" offset */
		end = queue.GetLength();

	if (start > end)
		/* an invalid "start" offset is fatal */
		return false;

	queue_print_info(client, queue, start, end);
	return true;
}

bool
playlist_print_id(Client &client, const playlist &playlist,
		  unsigned id)
{
	int position;

	position = playlist.queue.IdToPosition(id);
	if (position < 0)
		/* no such song */
		return false;

	return playlist_print_info(client, playlist, position, position + 1);
}

bool
playlist_print_current(Client &client, const playlist &playlist)
{
	int current_position = playlist.GetCurrentPosition();
	if (current_position < 0)
		return false;

	queue_print_info(client, playlist.queue,
			 current_position, current_position + 1);
	return true;
}

void
playlist_print_find(Client &client, const playlist &playlist,
		    const SongFilter &filter)
{
	queue_find(client, playlist.queue, filter);
}

void
playlist_print_changes_info(Client &client,
			    const playlist &playlist,
			    uint32_t version)
{
	queue_print_changes_info(client, playlist.queue, version);
}

void
playlist_print_changes_position(Client &client,
				const playlist &playlist,
				uint32_t version)
{
	queue_print_changes_position(client, playlist.queue, version);
}

#ifdef ENABLE_DATABASE

static bool
PrintSongDetails(Client &client, const char *uri_utf8)
{
	const Database *db = client.partition.instance.database;
	if (db == nullptr)
		return false;

	auto *song = db->GetSong(uri_utf8, IgnoreError());
	if (song == nullptr)
		return false;

	song_print_info(client, *song);
	db->ReturnSong(song);
	return true;
}

#endif

bool
spl_print(Client &client, const char *name_utf8, bool detail,
	  Error &error)
{
#ifndef ENABLE_DATABASE
	(void)detail;
#endif

	PlaylistFileContents contents = LoadPlaylistFile(name_utf8, error);
	if (contents.empty() && error.IsDefined())
		return false;

	for (const auto &uri_utf8 : contents) {
#ifdef ENABLE_DATABASE
		if (!detail || !PrintSongDetails(client, uri_utf8.c_str()))
#endif
			client_printf(client, SONG_FILE "%s\n",
				      uri_utf8.c_str());
	}

	return true;
}
