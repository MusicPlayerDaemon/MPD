/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "playlist/flac_playlist_plugin.h"
#include "playlist_plugin.h"
#include "tag.h"
#include "song.h"
#include "decoder/flac_metadata.h"

#include <FLAC/metadata.h>

#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "flac"

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7

struct flac_playlist {
	struct playlist_provider base;

	char *uri;

	FLAC__StreamMetadata *cuesheet;
	FLAC__StreamMetadata streaminfo;

	unsigned next_track;
};

static struct playlist_provider *
flac_playlist_open_uri(const char *uri)
{
	if (!g_path_is_absolute(uri))
		/* only local files supported */
		return NULL;

	FLAC__StreamMetadata *cuesheet;
	if (!FLAC__metadata_get_cuesheet(uri, &cuesheet))
		return NULL;

	struct flac_playlist *playlist = g_new(struct flac_playlist, 1);
	playlist_provider_init(&playlist->base, &flac_playlist_plugin);

	if (!FLAC__metadata_get_streaminfo(uri, &playlist->streaminfo)) {
		FLAC__metadata_object_delete(playlist->cuesheet);
		g_free(playlist);
		return NULL;
	}

	playlist->uri = g_strdup(uri);
	playlist->cuesheet = cuesheet;
	playlist->next_track = 0;

	return &playlist->base;
}

static void
flac_playlist_close(struct playlist_provider *_playlist)
{
	struct flac_playlist *playlist = (struct flac_playlist *)_playlist;

	g_free(playlist->uri);
	FLAC__metadata_object_delete(playlist->cuesheet);
	g_free(playlist);
}

static struct song *
flac_playlist_read(struct playlist_provider *_playlist)
{
	struct flac_playlist *playlist = (struct flac_playlist *)_playlist;
	const FLAC__StreamMetadata_CueSheet *cs =
		&playlist->cuesheet->data.cue_sheet;

	/* find the next audio track */

	while (playlist->next_track < cs->num_tracks &&
	       (cs->tracks[playlist->next_track].number > cs->num_tracks ||
		cs->tracks[playlist->next_track].type != 0))
		++playlist->next_track;

	if (playlist->next_track >= cs->num_tracks)
		return NULL;

	FLAC__uint64 start = cs->tracks[playlist->next_track].offset;
	++playlist->next_track;
	FLAC__uint64 end = playlist->next_track < cs->num_tracks
		? cs->tracks[playlist->next_track].offset
		: playlist->streaminfo.data.stream_info.total_samples;

	struct song *song = song_file_new(playlist->uri, NULL);
	song->start_ms = start * 1000 /
		playlist->streaminfo.data.stream_info.sample_rate;
	song->end_ms = end * 1000 /
		playlist->streaminfo.data.stream_info.sample_rate;

	char track[16];
	g_snprintf(track, sizeof(track), "%u", playlist->next_track);
	song->tag = flac_tag_load(playlist->uri, track);
	if (song->tag == NULL)
		song->tag = tag_new();

	song->tag->time = end > start
		? ((end - start - 1 +
		    playlist->streaminfo.data.stream_info.sample_rate) /
		   playlist->streaminfo.data.stream_info.sample_rate)
		: 0;

	tag_clear_items_by_type(song->tag, TAG_TRACK);
	tag_add_item(song->tag, TAG_TRACK, track);

	return song;
}

static const char *const flac_playlist_suffixes[] = {
	"flac",
	NULL
};

static const char *const flac_playlist_mime_types[] = {
	"application/flac",
	"application/x-flac",
	"audio/flac",
	"audio/x-flac",
	NULL
};

const struct playlist_plugin flac_playlist_plugin = {
	.name = "flac",

	.open_uri = flac_playlist_open_uri,
	.close = flac_playlist_close,
	.read = flac_playlist_read,

	.suffixes = flac_playlist_suffixes,
	.mime_types = flac_playlist_mime_types,
};

#else /* FLAC_API_VERSION_CURRENT <= 7 */

static bool
flac_playlist_init(G_GNUC_UNUSED const struct config_param *param)
{
	/* this libFLAC version does not support embedded CUE sheets;
	   disable this plugin */
	return false;
}

const struct playlist_plugin flac_playlist_plugin = {
	.name = "flac",
	.init = flac_playlist_init,
};

#endif
