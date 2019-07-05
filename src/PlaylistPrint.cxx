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

#include "PlaylistPrint.hxx"
#include "PlaylistFile.hxx"
#include "PlaylistError.hxx"
#include "queue/Playlist.hxx"
#include "queue/QueuePrint.hxx"

#define SONG_FILE "file: "
#define SONG_TIME "Time: "

void
playlist_print_uris(Response &r, const playlist &playlist)
{
	const Queue &queue = playlist.queue;

	queue_print_uris(r, queue, 0, queue.GetLength());
}

void
playlist_print_info(Response &r, const playlist &playlist,
		    unsigned start, unsigned end)
{
	const Queue &queue = playlist.queue;

	if (end > queue.GetLength())
		/* correct the "end" offset */
		end = queue.GetLength();

	if (start > end)
		/* an invalid "start" offset is fatal */
		throw PlaylistError::BadRange();

	queue_print_info(r, queue, start, end);
}

void
playlist_print_id(Response &r, const playlist &playlist,
		  unsigned id)
{
	int position;

	position = playlist.queue.IdToPosition(id);
	if (position < 0)
		/* no such song */
		throw PlaylistError::NoSuchSong();

	playlist_print_info(r, playlist, position, position + 1);
}

bool
playlist_print_current(Response &r, const playlist &playlist)
{
	int current_position = playlist.GetCurrentPosition();
	if (current_position < 0)
		return false;

	queue_print_info(r, playlist.queue,
			 current_position, current_position + 1);
	return true;
}

void
playlist_print_find(Response &r, const playlist &playlist,
		    const SongFilter &filter)
{
	queue_find(r, playlist.queue, filter);
}

void
playlist_print_changes_info(Response &r, const playlist &playlist,
			    uint32_t version,
			    unsigned start, unsigned end)
{
	queue_print_changes_info(r, playlist.queue, version,
				 start, end);
}

void
playlist_print_changes_position(Response &r,
				const playlist &playlist,
				uint32_t version,
				unsigned start, unsigned end)
{
	queue_print_changes_position(r, playlist.queue, version,
				     start, end);
}
