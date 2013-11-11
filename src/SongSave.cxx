/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "SongSave.hxx"
#include "Song.hxx"
#include "TagSave.hxx"
#include "Directory.hxx"
#include "TextFile.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <string.h>
#include <stdlib.h>

#define SONG_MTIME "mtime"
#define SONG_END "song_end"

static constexpr Domain song_save_domain("song_save");

void
song_save(FILE *fp, const Song &song)
{
	fprintf(fp, SONG_BEGIN "%s\n", song.uri);

	if (song.end_ms > 0)
		fprintf(fp, "Range: %u-%u\n", song.start_ms, song.end_ms);
	else if (song.start_ms > 0)
		fprintf(fp, "Range: %u-\n", song.start_ms);

	if (song.tag != nullptr)
		tag_save(fp, *song.tag);

	fprintf(fp, SONG_MTIME ": %li\n", (long)song.mtime);
	fprintf(fp, SONG_END "\n");
}

Song *
song_load(TextFile &file, Directory *parent, const char *uri,
	  Error &error)
{
	Song *song = parent != nullptr
		? Song::NewFile(uri, parent)
		: Song::NewRemote(uri);
	char *line, *colon;
	TagType type;
	const char *value;

	TagBuilder tag;

	while ((line = file.ReadLine()) != nullptr &&
	       strcmp(line, SONG_END) != 0) {
		colon = strchr(line, ':');
		if (colon == nullptr || colon == line) {
			song->Free();

			error.Format(song_save_domain,
				     "unknown line in db: %s", line);
			return nullptr;
		}

		*colon++ = 0;
		value = strchug_fast(colon);

		if ((type = tag_name_parse(line)) != TAG_NUM_OF_ITEM_TYPES) {
			tag.AddItem(type, value);
		} else if (strcmp(line, "Time") == 0) {
			tag.SetTime(atoi(value));
		} else if (strcmp(line, "Playlist") == 0) {
			tag.SetHasPlaylist(strcmp(value, "yes") == 0);
		} else if (strcmp(line, SONG_MTIME) == 0) {
			song->mtime = atoi(value);
		} else if (strcmp(line, "Range") == 0) {
			char *endptr;

			song->start_ms = strtoul(value, &endptr, 10);
			if (*endptr == '-')
				song->end_ms = strtoul(endptr + 1, nullptr, 10);
		} else {
			song->Free();

			error.Format(song_save_domain,
				     "unknown line in db: %s", line);
			return nullptr;
		}
	}

	if (tag.IsDefined())
		song->tag = tag.Commit();

	return song;
}
