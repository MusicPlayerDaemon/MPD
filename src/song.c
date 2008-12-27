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
#include "utils.h"
#include "log.h"
#include "mapper.h"
#include "path.h"
#include "playlist.h"
#include "decoder_list.h"
#include "decoder_api.h"

static struct song *
song_alloc(const char *url, struct directory *parent)
{
	size_t urllen;
	struct song *song;

	assert(url);
	urllen = strlen(url);
	assert(urllen);
	song = xmalloc(sizeof(*song) - sizeof(song->url) + urllen + 1);

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

	if (strchr(path, '\n')) {
		DEBUG("newSong: '%s' is not a valid uri\n", path);
		return NULL;
	}

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
	free(song);
}

bool
song_file_update(struct song *song)
{
	char buffer[MPD_PATH_MAX];
	const char *path_fs;
	const struct decoder_plugin *plugin;
	unsigned int next = 0;
	struct stat st;

	assert(song_is_file(song));

	path_fs = map_song_fs(song, buffer);
	if (path_fs == NULL)
		return false;

	if (song->tag != NULL) {
		tag_free(song->tag);
		song->tag = NULL;
	}

	if (stat(path_fs, &st) < 0 || !S_ISREG(st.st_mode))
		return false;

	song->mtime = st.st_mtime;

	while (song->tag == NULL &&
	       (plugin = hasMusicSuffix(path_fs, next++)))
		song->tag = plugin->tag_dup(path_fs);

	return song->tag != NULL;
}

bool
song_file_update_inarchive(struct song *song)
{
	char buffer[MPD_PATH_MAX];
	const char *path_fs;
	const struct decoder_plugin *plugin;

	assert(song_is_file(song));

	path_fs = map_song_fs(song, buffer);
	if (path_fs == NULL)
		return false;

	if (song->tag != NULL) {
		tag_free(song->tag);
		song->tag = NULL;
	}
	//accept every file that has music suffix
	//because we dont support tag reading throught
	//input streams
	plugin = hasMusicSuffix(path_fs, 0);
	if (plugin) {
		song->tag = tag_new();
		//tag_add_item(tag, TAG_ITEM_TITLE, f->title);
	}
	return song->tag != NULL;
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
