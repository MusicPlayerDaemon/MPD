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

#include "QueuePrint.hxx"
#include "Queue.hxx"
#include "song/Filter.hxx"
#include "SongPrint.hxx"
#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"
#include "client/Response.hxx"

/**
 * Send detailed information about a range of songs in the queue to a
 * client.
 *
 * @param client the client which has requested information
 * @param start the index of the first song (including)
 * @param end the index of the last song (excluding)
 */
static void
queue_print_song_info(Response &r, const Queue &queue,
		      unsigned position)
{
	song_print_info(r, queue.Get(position));
	r.Format("Pos: %u\nId: %u\n",
		 position, queue.PositionToId(position));

	uint8_t priority = queue.GetPriorityAtPosition(position);
	if (priority != 0)
		r.Format("Prio: %u\n", priority);
}

void
queue_print_info(Response &r, const Queue &queue,
		 unsigned start, unsigned end)
{
	assert(start <= end);
	assert(end <= queue.GetLength());

	for (unsigned i = start; i < end; ++i)
		queue_print_song_info(r, queue, i);
}

void
queue_print_uris(Response &r, const Queue &queue,
		 unsigned start, unsigned end)
{
	assert(start <= end);
	assert(end <= queue.GetLength());

	for (unsigned i = start; i < end; ++i) {
		r.Format("%i:", i);
		song_print_uri(r, queue.Get(i));
	}
}

void
queue_print_changes_info(Response &r, const Queue &queue,
			 uint32_t version,
			 unsigned start, unsigned end)
{
	assert(start <= end);

	if (start >= queue.GetLength())
		return;

	if (end > queue.GetLength())
		end = queue.GetLength();

	for (unsigned i = start; i < end; i++)
		if (queue.IsNewerAtPosition(i, version))
			queue_print_song_info(r, queue, i);
}

void
queue_print_changes_position(Response &r, const Queue &queue,
			     uint32_t version,
			     unsigned start, unsigned end)
{
	assert(start <= end);

	if (start >= queue.GetLength())
		return;

	if (end > queue.GetLength())
		end = queue.GetLength();

	for (unsigned i = start; i < end; i++)
		if (queue.IsNewerAtPosition(i, version))
			r.Format("cpos: %i\nId: %i\n",
				 i, queue.PositionToId(i));
}

void
queue_find(Response &r, const Queue &queue,
	   const SongFilter &filter)
{
	for (unsigned i = 0; i < queue.GetLength(); i++) {
		const LightSong song{queue.Get(i)};

		if (filter.Match(song))
			queue_print_song_info(r, queue, i);
	}
}
