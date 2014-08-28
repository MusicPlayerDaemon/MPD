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
#include "QueueSave.hxx"
#include "Queue.hxx"
#include "PlaylistError.hxx"
#include "DetachedSong.hxx"
#include "SongSave.hxx"
#include "SongLoader.hxx"
#include "playlist/PlaylistSong.hxx"
#include "fs/io/TextFile.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "fs/Traits.hxx"
#include "Log.hxx"

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

void
queue_load_song(TextFile &file, const SongLoader &loader,
		const char *line, Queue &queue)
{
	if (queue.IsFull())
		return;

	uint8_t priority = 0;
	if (StringStartsWith(line, PRIO_LABEL)) {
		priority = strtoul(line + sizeof(PRIO_LABEL) - 1, nullptr, 10);

		line = file.ReadLine();
		if (line == nullptr)
			return;
	}

	DetachedSong *song;

	if (StringStartsWith(line, SONG_BEGIN)) {
		const char *uri = line + sizeof(SONG_BEGIN) - 1;

		Error error;
		song = song_load(file, uri, error);
		if (song == nullptr) {
			LogError(error);
			return;
		}
	} else {
		char *endptr;
		long ret = strtol(line, &endptr, 10);
		if (ret < 0 || *endptr != ':' || endptr[1] == 0) {
			LogError(playlist_domain,
				 "Malformed playlist line in state file");
			return;
		}

		const char *uri = endptr + 1;

		song = new DetachedSong(uri);
	}

	if (!playlist_check_translate_song(*song, nullptr, loader)) {
		delete song;
		return;
	}

	queue.Append(std::move(*song), priority);
	delete song;
}
