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

#include "config.h"
#include "directory.h"
#include "song.h"
#include "path.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct directory *
directory_new(const char *path, struct directory *parent)
{
	struct directory *directory;
	size_t pathlen = strlen(path);

	assert(path != NULL);
	assert((*path == 0) == (parent == NULL));

	directory = g_malloc0(sizeof(*directory) -
			      sizeof(directory->path) + pathlen + 1);
	directory->parent = parent;
	memcpy(directory->path, path, pathlen + 1);

	return directory;
}

void
directory_free(struct directory *directory)
{
	for (unsigned i = 0; i < directory->songs.nr; ++i)
		song_free(directory->songs.base[i]);

	for (unsigned i = 0; i < directory->children.nr; ++i)
		directory_free(directory->children.base[i]);

	dirvec_destroy(&directory->children);
	songvec_destroy(&directory->songs);
	g_free(directory);
	/* this resets last dir returned */
	/*directory_get_path(NULL); */
}

const char *
directory_get_name(const struct directory *directory)
{
	return g_basename(directory->path);
}

void
directory_prune_empty(struct directory *directory)
{
	int i;
	struct dirvec *dv = &directory->children;

	for (i = dv->nr; --i >= 0; ) {
		struct directory *child = dv->base[i];

		directory_prune_empty(child);

		if (directory_is_empty(child)) {
			dirvec_delete(dv, child);
			directory_free(child);
		}
	}
	if (!dv->nr)
		dirvec_destroy(dv);
}

struct directory *
directory_lookup_directory(struct directory *directory, const char *uri)
{
	struct directory *cur = directory;
	struct directory *found = NULL;
	char *duplicated;
	char *locate;

	assert(uri != NULL);

	if (isRootDirectory(uri))
		return directory;

	duplicated = g_strdup(uri);
	locate = strchr(duplicated, '/');
	while (1) {
		if (locate)
			*locate = '\0';
		if (!(found = directory_get_child(cur, duplicated)))
			break;
		assert(cur == found->parent);
		cur = found;
		if (!locate)
			break;
		*locate = '/';
		locate = strchr(locate + 1, '/');
	}

	g_free(duplicated);

	return found;
}

struct song *
directory_lookup_song(struct directory *directory, const char *uri)
{
	char *duplicated, *base;
	struct song *song;

	assert(directory != NULL);
	assert(uri != NULL);

	duplicated = g_strdup(uri);
	base = strrchr(duplicated, '/');

	if (base != NULL) {
		*base++ = 0;
		directory = directory_lookup_directory(directory, duplicated);
		if (directory == NULL) {
			g_free(duplicated);
			return NULL;
		}
	} else
		base = duplicated;

	song = songvec_find(&directory->songs, base);
	assert(song == NULL || song->parent == directory);

	g_free(duplicated);
	return song;

}

void
directory_sort(struct directory *directory)
{
	int i;
	struct dirvec *dv = &directory->children;

	dirvec_sort(dv);
	songvec_sort(&directory->songs);

	for (i = dv->nr; --i >= 0; )
		directory_sort(dv->base[i]);
}

int
directory_walk(struct directory *directory,
	       int (*forEachSong)(struct song *, void *),
	       int (*forEachDir)(struct directory *, void *),
	       void *data)
{
	struct dirvec *dv = &directory->children;
	int err = 0;
	size_t j;

	if (forEachDir && (err = forEachDir(directory, data)) < 0)
		return err;

	if (forEachSong) {
		err = songvec_for_each(&directory->songs, forEachSong, data);
		if (err < 0)
			return err;
	}

	for (j = 0; err >= 0 && j < dv->nr; ++j)
		err = directory_walk(dv->base[j], forEachSong,
						forEachDir, data);

	return err;
}
