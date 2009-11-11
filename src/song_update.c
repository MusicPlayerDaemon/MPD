/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "config.h" /* must be first for large file support */
#include "song.h"
#include "uri.h"
#include "directory.h"
#include "mapper.h"
#include "decoder_list.h"
#include "decoder_plugin.h"
#include "tag_ape.h"
#include "tag_id3.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

struct song *
song_file_load(const char *path, struct directory *parent)
{
	struct song *song;
	bool ret;

	assert((parent == NULL) == g_path_is_absolute(path));
	assert(!uri_has_scheme(path));
	assert(strchr(path, '\n') == NULL);

	song = song_file_new(path, parent);

	//in archive ?
	if (parent != NULL && parent->device == DEVICE_INARCHIVE) {
		ret = song_file_update_inarchive(song);
	} else {
		ret = song_file_update(song);
	}
	if (!ret) {
		song_free(song);
		return NULL;
	}

	return song;
}

/**
 * Attempts to load APE or ID3 tags from the specified file.
 */
static struct tag *
tag_load_fallback(const char *path)
{
	struct tag *tag = tag_ape_load(path);
	if (tag == NULL)
		tag = tag_id3_load(path);
	return tag;
}

/**
 * The decoder plugin failed to load any tags: fall back to the APE or
 * ID3 tag loader.
 */
static struct tag *
tag_fallback(const char *path, struct tag *tag)
{
	struct tag *fallback = tag_load_fallback(path);

	if (fallback != NULL) {
		/* tag was successfully loaded: copy the song
		   duration, and destroy the old (empty) tag */
		fallback->time = tag->time;
		tag_free(tag);
		return fallback;
	} else
		/* no APE/ID3 tag found: return the empty tag */
		return tag;
}

bool
song_file_update(struct song *song)
{
	const char *suffix;
	char *path_fs;
	const struct decoder_plugin *plugin;
	struct stat st;

	assert(song_is_file(song));

	/* check if there's a suffix and a plugin */

	suffix = uri_get_suffix(song->uri);
	if (suffix == NULL)
		return false;

	plugin = decoder_plugin_from_suffix(suffix, NULL);
	if (plugin == NULL)
		return false;

	path_fs = map_song_fs(song);
	if (path_fs == NULL)
		return false;

	if (song->tag != NULL) {
		tag_free(song->tag);
		song->tag = NULL;
	}

	if (stat(path_fs, &st) < 0 || !S_ISREG(st.st_mode)) {
		g_free(path_fs);
		return false;
	}

	song->mtime = st.st_mtime;

	do {
		song->tag = plugin->tag_dup(path_fs);
		if (song->tag != NULL)
			break;

		plugin = decoder_plugin_from_suffix(suffix, plugin);
	} while (plugin != NULL);

	if (song->tag != NULL && tag_is_empty(song->tag))
		song->tag = tag_fallback(path_fs, song->tag);

	g_free(path_fs);
	return song->tag != NULL;
}

bool
song_file_update_inarchive(struct song *song)
{
	const char *suffix;
	const struct decoder_plugin *plugin;

	assert(song_is_file(song));

	/* check if there's a suffix and a plugin */

	suffix = uri_get_suffix(song->uri);
	if (suffix == NULL)
		return false;

	plugin = decoder_plugin_from_suffix(suffix, false);
	if (plugin == NULL)
		return false;

	if (song->tag != NULL)
		tag_free(song->tag);

	//accept every file that has music suffix
	//because we dont support tag reading throught
	//input streams
	song->tag = tag_new();

	return true;
}
