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

/** \file
 *
 * Playlist plugin that reads embedded cue sheets from the "CUESHEET"
 * tag of a music file.
 */

#include "config.h"
#include "EmbeddedCuePlaylistPlugin.hxx"
#include "PlaylistPlugin.hxx"
#include "SongEnumerator.hxx"
#include "tag/Tag.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagId3.hxx"
#include "tag/ApeTag.hxx"
#include "Song.hxx"
#include "TagFile.hxx"
#include "cue/CueParser.hxx"
#include "fs/Traits.hxx"

#include <glib.h>
#include <assert.h>
#include <string.h>

class EmbeddedCuePlaylist final : public SongEnumerator {
public:
	/**
	 * This is an override for the CUE's "FILE".  An embedded CUE
	 * sheet must always point to the song file it is contained
	 * in.
	 */
	char *filename;

	/**
	 * The value of the file's "CUESHEET" tag.
	 */
	char *cuesheet;

	/**
	 * The offset of the next line within "cuesheet".
	 */
	char *next;

	CueParser *parser;

public:
	EmbeddedCuePlaylist()
		:filename(nullptr), cuesheet(nullptr), parser(nullptr) {
	}

	virtual ~EmbeddedCuePlaylist() {
		delete parser;
		g_free(cuesheet);
		g_free(filename);
	}

	virtual Song *NextSong() override;
};

static void
embcue_tag_pair(const char *name, const char *value, void *ctx)
{
	EmbeddedCuePlaylist *playlist = (EmbeddedCuePlaylist *)ctx;

	if (playlist->cuesheet == NULL &&
	    g_ascii_strcasecmp(name, "cuesheet") == 0)
		playlist->cuesheet = g_strdup(value);
}

static const struct tag_handler embcue_tag_handler = {
	nullptr,
	nullptr,
	embcue_tag_pair,
};

static SongEnumerator *
embcue_playlist_open_uri(const char *uri,
			 gcc_unused Mutex &mutex,
			 gcc_unused Cond &cond)
{
	if (!PathTraits::IsAbsoluteUTF8(uri))
		/* only local files supported */
		return NULL;

	const auto playlist = new EmbeddedCuePlaylist();

	tag_file_scan(uri, &embcue_tag_handler, playlist);
	if (playlist->cuesheet == NULL) {
		tag_ape_scan2(uri, &embcue_tag_handler, playlist);
		if (playlist->cuesheet == NULL)
			tag_id3_scan(uri, &embcue_tag_handler, playlist);
	}

	if (playlist->cuesheet == NULL) {
		/* no "CUESHEET" tag found */
		delete playlist;
		return NULL;
	}

	playlist->filename = g_path_get_basename(uri);

	playlist->next = playlist->cuesheet;
	playlist->parser = new CueParser();

	return playlist;
}

Song *
EmbeddedCuePlaylist::NextSong()
{
	Song *song = parser->Get();
	if (song != NULL)
		return song;

	while (*next != 0) {
		const char *line = next;
		char *eol = strpbrk(next, "\r\n");
		if (eol != NULL) {
			/* null-terminate the line */
			*eol = 0;
			next = eol + 1;
		} else
			/* last line; put the "next" pointer to the
			   end of the buffer */
			next += strlen(line);

		parser->Feed(line);
		song = parser->Get();
		if (song != NULL)
			return song->ReplaceURI(filename);
	}

	parser->Finish();
	song = parser->Get();
	if (song != NULL)
		song = song->ReplaceURI(filename);
	return song;
}

static const char *const embcue_playlist_suffixes[] = {
	/* a few codecs that are known to be supported; there are
	   probably many more */
	"flac",
	"mp3", "mp2",
	"mp4", "mp4a", "m4b",
	"ape",
	"wv",
	"ogg", "oga",
	NULL
};

const struct playlist_plugin embcue_playlist_plugin = {
	"cue",

	nullptr,
	nullptr,
	embcue_playlist_open_uri,
	nullptr,

	embcue_playlist_suffixes,
	nullptr,
	nullptr,
};
