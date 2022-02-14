/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Save.hxx"
#include "Queue.hxx"
#include "PlaylistError.hxx"
#include "song/DetachedSong.hxx"
#include "SongSave.hxx"
#include "playlist/PlaylistSong.hxx"
#include "io/LineReader.hxx"
#include "io/BufferedOutputStream.hxx"
#include "util/StringCompare.hxx"
#include "Log.hxx"

#include <exception>

#include <stdlib.h>

#define PRIO_LABEL "Prio: "

static void
queue_save_database_song(BufferedOutputStream &os,
			 int idx, const DetachedSong &song)
{
	os.Format("%i:%s\n", idx, song.GetURI());
}

static void
queue_save_full_song(BufferedOutputStream &os, const DetachedSong &song)
{
	song_save(os, song);
}

static void
queue_save_song(BufferedOutputStream &os, int idx, const DetachedSong &song)
{
	if (song.IsInDatabase() &&
	    song.GetStartTime().IsZero() && song.GetEndTime().IsZero())
		/* use the brief format (just the URI) for "full"
		   database songs */
		queue_save_database_song(os, idx, song);
	else
		/* use the long format (URI, range, tags) for the
		   rest, so all metadata survives a MPD restart */
		queue_save_full_song(os, song);
}

void
queue_save(BufferedOutputStream &os, const Queue &queue)
{
	for (unsigned i = 0; i < queue.GetLength(); i++) {
		uint8_t prio = queue.GetPriorityAtPosition(i);
		if (prio != 0)
			os.Format(PRIO_LABEL "%u\n", prio);

		queue_save_song(os, i, queue.Get(i));
	}
}

static DetachedSong
LoadQueueSong(LineReader &file, const char *line)
{
	std::unique_ptr<DetachedSong> song;

	if (const char *p = StringAfterPrefix(line, SONG_BEGIN)) {
		const char *uri = p;
		return song_load(file, uri);
	} else {
		char *endptr;
		long ret = strtol(line, &endptr, 10);
		if (ret < 0 || *endptr != ':' || endptr[1] == 0)
			throw std::runtime_error("Malformed playlist line in state file");

		const char *uri = endptr + 1;

		return DetachedSong(uri);
	}
}

void
queue_load_song(LineReader &file, const SongLoader &loader,
		const char *line, Queue &queue)
{
	if (queue.IsFull())
		return;

	uint8_t priority = 0;
	const char *p;
	if ((p = StringAfterPrefix(line, PRIO_LABEL))) {
		priority = strtoul(p, nullptr, 10);

		line = file.ReadLine();
		if (line == nullptr)
			return;
	}

	auto song = LoadQueueSong(file, line);

	if (!playlist_check_translate_song(song, {}, loader))
		return;

	queue.Append(std::move(song), priority);
}
