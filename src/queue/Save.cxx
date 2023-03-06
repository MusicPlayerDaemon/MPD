// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

#include <fmt/format.h>

#include <exception>

#include <stdlib.h>

#define PRIO_LABEL "Prio: "

static void
queue_save_database_song(BufferedOutputStream &os,
			 int idx, const DetachedSong &song)
{
	os.Fmt(FMT_STRING("{}:{}\n"), idx, song.GetURI());
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
			os.Fmt(FMT_STRING(PRIO_LABEL "{}\n"), prio);

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
