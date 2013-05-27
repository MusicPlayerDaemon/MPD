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
#include "tag_handler.h"
#include "input_stream.h"

#include <glib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

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
static bool
tag_scan_fallback(const char *path,
		  const struct tag_handler *handler, void *handler_ctx)
{
	return tag_ape_scan2(path, handler, handler_ctx) ||
		tag_id3_scan(path, handler, handler_ctx);
}

bool
song_file_update(struct song *song)
{
	const char *suffix;
	char *path_fs;
	const struct decoder_plugin *plugin;
	struct stat st;
	struct input_stream *is = NULL;

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

	GMutex *mutex = NULL;
	GCond *cond;
#if !GCC_CHECK_VERSION(4, 2)
	/* work around "may be used uninitialized in this function"
	   false positive */
	cond = NULL;
#endif

	do {
		/* load file tag */
		song->tag = tag_new();
		if (decoder_plugin_scan_file(plugin, path_fs,
					     &full_tag_handler, song->tag))
			break;

		tag_free(song->tag);
		song->tag = NULL;

		/* fall back to stream tag */
		if (plugin->scan_stream != NULL) {
			/* open the input_stream (if not already
			   open) */
			if (is == NULL) {
				mutex = g_mutex_new();
				cond = g_cond_new();
				is = input_stream_open(path_fs, mutex, cond,
						       NULL);
			}

			/* now try the stream_tag() method */
			if (is != NULL) {
				song->tag = tag_new();
				if (decoder_plugin_scan_stream(plugin, is,
							       &full_tag_handler,
							       song->tag))
					break;

				tag_free(song->tag);
				song->tag = NULL;

				input_stream_lock_seek(is, 0, SEEK_SET, NULL);
			}
		}

		plugin = decoder_plugin_from_suffix(suffix, plugin);
	} while (plugin != NULL);

	if (is != NULL)
		input_stream_close(is);

	if (mutex != NULL) {
		g_cond_free(cond);
		g_mutex_free(mutex);
	}

	if (song->tag != NULL && tag_is_empty(song->tag))
		tag_scan_fallback(path_fs, &full_tag_handler, song->tag);

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

	plugin = decoder_plugin_from_suffix(suffix, NULL);
	if (plugin == NULL)
		return false;

	if (song->tag != NULL)
		tag_free(song->tag);

	//accept every file that has music suffix
	//because we don't support tag reading through
	//input streams
	song->tag = tag_new();

	return true;
}
