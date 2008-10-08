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
#include "path.h"
#include "playlist.h"
#include "decoder_list.h"
#include "decoder_api.h"

#include "os_compat.h"

Song *
song_alloc(const char *url, struct directory *parent)
{
	size_t urllen;
	Song *song;

	assert(url);
	urllen = strlen(url);
	assert(urllen);
	song = xmalloc(sizeof(*song) - sizeof(song->url) + urllen + 1);

	song->tag = NULL;
	memcpy(song->url, url, urllen + 1);
	song->parentDir = parent;

	return song;
}

Song *newSong(const char *url, struct directory *parentDir)
{
	Song *song;
	assert(*url);

	if (strchr(url, '\n')) {
		DEBUG("newSong: '%s' is not a valid uri\n", url);
		return NULL;
	}

	song = song_alloc(url, parentDir);

	if (song_is_file(song)) {
		struct decoder_plugin *plugin;
		unsigned int next = 0;
		char path_max_tmp[MPD_PATH_MAX];
		char *abs_path = rmp2amp_r(path_max_tmp,
		                           get_song_url(path_max_tmp, song));

		while (!song->tag && (plugin = isMusic(abs_path,
						       &(song->mtime),
						       next++))) {
			song->tag = plugin->tag_dup(abs_path);
		}
		if (!song->tag || song->tag->time < 0) {
			freeJustSong(song);
			song = NULL;
		}
	}

	return song;
}

void freeJustSong(Song * song)
{
	if (song->tag)
		tag_free(song->tag);
	free(song);
}

int updateSongInfo(Song * song)
{
	if (song_is_file(song)) {
		struct decoder_plugin *plugin;
		unsigned int next = 0;
		char path_max_tmp[MPD_PATH_MAX];
		char abs_path[MPD_PATH_MAX];

		utf8_to_fs_charset(abs_path, get_song_url(path_max_tmp, song));
		rmp2amp_r(abs_path, abs_path);

		if (song->tag)
			tag_free(song->tag);

		song->tag = NULL;

		while (!song->tag && (plugin = isMusic(abs_path,
						       &(song->mtime),
						       next++))) {
			song->tag = plugin->tag_dup(abs_path);
		}
		if (!song->tag || song->tag->time < 0)
			return -1;
	}

	return 0;
}

char *get_song_url(char *path_max_tmp, Song *song)
{
	if (!song)
		return NULL;

	assert(*song->url);

	if (!song->parentDir || !song->parentDir->path)
		strcpy(path_max_tmp, song->url);
	else
		pfx_dir(path_max_tmp, song->url, strlen(song->url),
			getDirectoryPath(song->parentDir),
			strlen(getDirectoryPath(song->parentDir)));
	return path_max_tmp;
}
