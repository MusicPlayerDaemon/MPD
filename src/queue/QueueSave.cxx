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
#include "db/DatabaseSong.hxx"
#include "fs/TextFile.hxx"
#include "util/StringUtil.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "fs/Traits.hxx"
#include "Log.hxx"

#include <stdlib.h>

#define PRIO_LABEL "Prio: "

static void
queue_save_database_song(FILE *fp, int idx, const DetachedSong &song)
{
	fprintf(fp, "%i:%s\n", idx, song.GetURI());
}

static void
queue_save_full_song(FILE *fp, const DetachedSong &song)
{
	song_save(fp, song);
}

static void
queue_save_song(FILE *fp, int idx, const DetachedSong &song)
{
	if (song.IsInDatabase())
		queue_save_database_song(fp, idx, song);
	else
		queue_save_full_song(fp, song);
}

void
queue_save(FILE *fp, const Queue &queue)
{
	for (unsigned i = 0; i < queue.GetLength(); i++) {
		uint8_t prio = queue.GetPriorityAtPosition(i);
		if (prio != 0)
			fprintf(fp, PRIO_LABEL "%u\n", prio);

		queue_save_song(fp, i, queue.Get(i));
	}
}

void
queue_load_song(TextFile &file, const char *line, Queue &queue)
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
		if (!uri_has_scheme(uri) && !PathTraitsUTF8::IsAbsolute(uri))
			return;

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

		if (uri_has_scheme(uri)) {
			song = new DetachedSong(uri);
		} else {
#ifdef ENABLE_DATABASE
			song = DatabaseDetachSong(uri, IgnoreError());
			if (song == nullptr)
#endif
				return;
		}
	}

	queue.Append(std::move(*song), priority);
	delete song;
}
