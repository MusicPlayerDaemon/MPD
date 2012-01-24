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
#include "directory.h"
#include "song.h"
#include "path.h"
#include "util/list_sort.h"
#include "db_visitor.h"
#include "db_lock.h"

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
	INIT_LIST_HEAD(&directory->children);
	directory->parent = parent;
	memcpy(directory->path, path, pathlen + 1);

	playlist_vector_init(&directory->playlists);

	return directory;
}

void
directory_free(struct directory *directory)
{
	playlist_vector_deinit(&directory->playlists);

	for (unsigned i = 0; i < directory->songs.nr; ++i)
		song_free(directory->songs.base[i]);

	struct directory *child, *n;
	directory_for_each_child_safe(child, n, directory)
		directory_free(child);

	songvec_destroy(&directory->songs);
	g_free(directory);
	/* this resets last dir returned */
	/*directory_get_path(NULL); */
}

void
directory_delete(struct directory *directory)
{
	assert(directory != NULL);
	assert(directory->parent != NULL);

	db_lock();
	list_del(&directory->siblings);
	db_unlock();

	directory_free(directory);
}

const char *
directory_get_name(const struct directory *directory)
{
	return g_basename(directory->path);
}

struct directory *
directory_new_child(struct directory *parent, const char *name_utf8)
{
	assert(parent != NULL);
	assert(name_utf8 != NULL);
	assert(*name_utf8 != 0);

	char *allocated;
	const char *path_utf8;
	if (directory_is_root(parent)) {
		allocated = NULL;
		path_utf8 = name_utf8;
	} else {
		allocated = g_strconcat(directory_get_path(parent),
					"/", name_utf8, NULL);
		path_utf8 = allocated;
	}

	struct directory *directory = directory_new(path_utf8, parent);
	g_free(allocated);

	db_lock();
	list_add(&directory->siblings, &parent->children);
	db_unlock();
	return directory;
}

struct directory *
directory_get_child(const struct directory *directory, const char *name)
{
	db_lock();

	struct directory *child;
	directory_for_each_child(child, directory) {
		if (strcmp(directory_get_name(child), name) == 0) {
			db_unlock();
			return child;
		}
	}

	db_unlock();
	return NULL;
}

void
directory_prune_empty(struct directory *directory)
{
	struct directory *child, *n;
	directory_for_each_child_safe(child, n, directory) {
		directory_prune_empty(child);

		if (directory_is_empty(child))
			directory_delete(child);
	}
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

static int
directory_cmp(G_GNUC_UNUSED void *priv,
	      struct list_head *_a, struct list_head *_b)
{
	const struct directory *a = (const struct directory *)_a;
	const struct directory *b = (const struct directory *)_b;
	return g_utf8_collate(a->path, b->path);
}

void
directory_sort(struct directory *directory)
{
	db_lock();
	list_sort(NULL, &directory->children, directory_cmp);
	db_unlock();

	songvec_sort(&directory->songs);

	struct directory *child;
	directory_for_each_child(child, directory)
		directory_sort(child);
}

bool
directory_walk(const struct directory *directory, bool recursive,
	       const struct db_visitor *visitor, void *ctx,
	       GError **error_r)
{
	assert(directory != NULL);
	assert(visitor != NULL);
	assert(error_r == NULL || *error_r == NULL);

	if (visitor->song != NULL) {
		const struct songvec *sv = &directory->songs;
		for (size_t i = 0; i < sv->nr; ++i)
			if (!visitor->song(sv->base[i], ctx, error_r))
				return false;
	}

	if (visitor->playlist != NULL) {
		const struct playlist_vector *pv = &directory->playlists;
		for (const struct playlist_metadata *i = pv->head;
		     i != NULL; i = i->next)
			if (!visitor->playlist(i, directory, ctx, error_r))
				return false;
	}

	struct directory *child;
	directory_for_each_child(child, directory) {
		if (visitor->directory != NULL &&
		    !visitor->directory(child, ctx, error_r))
			return false;

		if (recursive &&
		    !directory_walk(child, recursive, visitor, ctx, error_r))
			return false;
	}

	return true;
}
