// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlaylistPrint.hxx"
#include "PlaylistFile.hxx"
#include "PlaylistError.hxx"
#include "queue/Playlist.hxx"
#include "queue/Print.hxx"
#include "protocol/RangeArg.hxx"

#define SONG_FILE "file: "
#define SONG_TIME "Time: "

void
playlist_print_uris(Response &r, const playlist &playlist)
{
	const Queue &queue = playlist.queue;

	queue_print_uris(r, queue, 0, queue.GetLength());
}

void
playlist_print_info(Response &r, const playlist &playlist, RangeArg range)
{
	const Queue &queue = playlist.queue;

	if (!range.CheckClip(queue.GetLength()))
		throw PlaylistError::BadRange();

	if (range.IsEmpty())
		return;

	queue_print_info(r, queue, range.start, range.end);
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

	playlist_print_info(r, playlist, {unsigned(position), position + 1U});
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
		    const QueueSelection &selection)
{
	PrintQueue(r, playlist.queue, selection);
}

void
playlist_print_changes_info(Response &r, const playlist &playlist,
			    uint32_t version,
			    RangeArg range)
{
	const Queue &queue = playlist.queue;
	range.ClipRelaxed(queue.GetLength());

	queue_print_changes_info(r, queue, version,
				 range.start, range.end);
}

void
playlist_print_changes_position(Response &r,
				const playlist &playlist,
				uint32_t version,
				RangeArg range)
{
	const Queue &queue = playlist.queue;
	range.ClipRelaxed(queue.GetLength());

	queue_print_changes_position(r, playlist.queue, version,
				     range.start, range.end);
}
