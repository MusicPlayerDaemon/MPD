/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
#include "playlist/cue_playlist_plugin.h"
#include "playlist_plugin.h"
#include "tag.h"
#include "song.h"
#include "cue/cue_tag.h"

#include <glib.h>
#include <libcue/libcue.h>
#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cue"

struct cue_playlist {
	struct playlist_provider base;

	struct Cd *cd;

	unsigned next;
};

static struct playlist_provider *
cue_playlist_open_uri(const char *uri)
{
	struct cue_playlist *playlist;
	FILE *file;
	struct Cd *cd;

	file = fopen(uri, "rt");
	if (file == NULL)
		return NULL;

	cd = cue_parse_file(file);
	fclose(file);
	if (cd == NULL)
		return NULL;

	playlist = g_new(struct cue_playlist, 1);
	playlist_provider_init(&playlist->base, &cue_playlist_plugin);
	playlist->cd = cd;
	playlist->next = 1;

	return &playlist->base;
}

static void
cue_playlist_close(struct playlist_provider *_playlist)
{
	struct cue_playlist *playlist = (struct cue_playlist *)_playlist;

	cd_delete(playlist->cd);
	g_free(playlist);
}

static struct song *
cue_playlist_read(struct playlist_provider *_playlist)
{
	struct cue_playlist *playlist = (struct cue_playlist *)_playlist;
	struct Track *track;
	struct tag *tag;
	const char *filename;
	struct song *song;

	track = cd_get_track(playlist->cd, playlist->next);
	if (track == NULL)
		return NULL;

	tag = cue_tag(playlist->cd, playlist->next);
	if (tag == NULL)
		return NULL;

	++playlist->next;

	filename = track_get_filename(track);
	if (*filename == 0 || filename[0] == '.' ||
	    strchr(filename, '/') != NULL) {
		/* unsafe characters found, bail out */
		tag_free(tag);
		return NULL;
	}

	song = song_remote_new(filename);
	song->tag = tag;
	song->start_ms = ((track_get_start(track)
			   + track_get_index(track, 1)
			   - track_get_zero_pre(track)) * 1000) / 75;

	/* append pregap of the next track to the end of this one */
	track = cd_get_track(playlist->cd, playlist->next);
	if (track != NULL)
		song->end_ms = ((track_get_start(track)
				 + track_get_index(track, 1)
				 - track_get_zero_pre(track)) * 1000) / 75;
	else
		song->end_ms = 0;

	return song;
}

static const char *const cue_playlist_suffixes[] = {
	"cue",
	NULL
};

static const char *const cue_playlist_mime_types[] = {
	"application/x-cue",
	NULL
};

const struct playlist_plugin cue_playlist_plugin = {
	.name = "cue",

	.open_uri = cue_playlist_open_uri,
	.close = cue_playlist_close,
	.read = cue_playlist_read,

	.suffixes = cue_playlist_suffixes,
	.mime_types = cue_playlist_mime_types,
};
