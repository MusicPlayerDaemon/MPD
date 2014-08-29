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
#include "SongSave.hxx"
#include "db/plugins/simple/Song.hxx"
#include "DetachedSong.hxx"
#include "TagSave.hxx"
#include "fs/io/TextFile.hxx"
#include "fs/io/BufferedOutputStream.hxx"
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

static void
range_save(BufferedOutputStream &os, unsigned start_ms, unsigned end_ms)
{
	if (end_ms > 0)
		os.Format("Range: %u-%u\n", start_ms, end_ms);
	else if (start_ms > 0)
		os.Format("Range: %u-\n", start_ms);
}

void
song_save(BufferedOutputStream &os, const Song &song)
{
	os.Format(SONG_BEGIN "%s\n", song.uri);

	range_save(os, song.start_time.ToMS(), song.end_time.ToMS());

	tag_save(os, song.tag);

	os.Format(SONG_MTIME ": %li\n", (long)song.mtime);
	os.Format(SONG_END "\n");
}

void
song_save(BufferedOutputStream &os, const DetachedSong &song)
{
	os.Format(SONG_BEGIN "%s\n", song.GetURI());

	range_save(os, song.GetStartTime().ToMS(), song.GetEndTime().ToMS());

	tag_save(os, song.GetTag());

	os.Format(SONG_MTIME ": %li\n", (long)song.GetLastModified());
	os.Format(SONG_END "\n");
}

DetachedSong *
song_load(TextFile &file, const char *uri,
	  Error &error)
{
	DetachedSong *song = new DetachedSong(uri);

	TagBuilder tag;

	char *line;
	while ((line = file.ReadLine()) != nullptr &&
	       strcmp(line, SONG_END) != 0) {
		char *colon = strchr(line, ':');
		if (colon == nullptr || colon == line) {
			delete song;

			error.Format(song_save_domain,
				     "unknown line in db: %s", line);
			return nullptr;
		}

		*colon++ = 0;
		const char *value = StripLeft(colon);

		TagType type;
		if ((type = tag_name_parse(line)) != TAG_NUM_OF_ITEM_TYPES) {
			tag.AddItem(type, value);
		} else if (strcmp(line, "Time") == 0) {
			tag.SetDuration(SignedSongTime::FromS(atof(value)));
		} else if (strcmp(line, "Playlist") == 0) {
			tag.SetHasPlaylist(strcmp(value, "yes") == 0);
		} else if (strcmp(line, SONG_MTIME) == 0) {
			song->SetLastModified(atoi(value));
		} else if (strcmp(line, "Range") == 0) {
			char *endptr;

			unsigned start_ms = strtoul(value, &endptr, 10);
			unsigned end_ms = *endptr == '-'
				? strtoul(endptr + 1, nullptr, 10)
				: 0;

			song->SetStartTime(SongTime::FromMS(start_ms));
			song->SetEndTime(SongTime::FromMS(end_ms));
		} else {
			delete song;

			error.Format(song_save_domain,
				     "unknown line in db: %s", line);
			return nullptr;
		}
	}

	song->SetTag(tag.Commit());
	return song;
}
