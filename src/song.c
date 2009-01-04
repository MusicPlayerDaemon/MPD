/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "song.h"
#include "ls.h"
#include "directory.h"
#include "mapper.h"
#include "path.h"
#include "playlist.h"
#include "decoder_list.h"
#include "decoder_api.h"

#include <glib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static struct song *
song_alloc(const char *url, struct directory *parent)
{
	size_t urllen;
	struct song *song;

	assert(url);
	urllen = strlen(url);
	assert(urllen);
	song = g_malloc(sizeof(*song) - sizeof(song->url) + urllen + 1);

	song->tag = NULL;
	memcpy(song->url, url, urllen + 1);
	song->parent = parent;

	return song;
}

struct song *
song_remote_new(const char *url)
{
	return song_alloc(url, NULL);
}

struct song *
song_file_new(const char *path, struct directory *parent)
{
	assert((parent == NULL) == (*path == '/'));

	return song_alloc(path, parent);
}

struct song *
song_file_load(const char *path, struct directory *parent)
{
	struct song *song;
	bool ret;

	assert((parent == NULL) == (*path == '/'));
	assert(!uri_has_scheme(path));
	assert(strchr(path, '\n') == NULL);

	song = song_file_new(path, parent);

	//in archive ?
	if (parent->device == DEVICE_INARCHIVE) {
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

void
song_free(struct song *song)
{
	if (song->tag)
		tag_free(song->tag);
	g_free(song);
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

	suffix = uri_get_suffix(song->url);
	if (suffix == NULL)
		return false;

	plugin = decoder_plugin_from_suffix(suffix, false);
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

		plugin = decoder_plugin_from_suffix(suffix, true);
	} while (plugin != NULL);

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

	suffix = uri_get_suffix(song->url);
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

char *
song_get_url(const struct song *song, char *path_max_tmp)
{
	assert(song != NULL);
	assert(*song->url);

	if (!song->parent || isRootDirectory(song->parent->path))
		strcpy(path_max_tmp, song->url);
	else
		pfx_dir(path_max_tmp, song->url, strlen(song->url),
			directory_get_path(song->parent),
			strlen(directory_get_path(song->parent)));
	return path_max_tmp;
}
