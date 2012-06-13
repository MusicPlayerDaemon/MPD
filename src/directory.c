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
#include "song_sort.h"
#include "playlist_vector.h"
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
	INIT_LIST_HEAD(&directory->songs);
	INIT_LIST_HEAD(&directory->playlists);

	directory->parent = parent;
	memcpy(directory->path, path, pathlen + 1);

	return directory;
}

void
directory_free(struct directory *directory)
{
	playlist_vector_deinit(&directory->playlists);

	struct song *song, *ns;
	directory_for_each_song_safe(song, ns, directory)
		song_free(song);

	struct directory *child, *n;
	directory_for_each_child_safe(child, n, directory)
		directory_free(child);

	g_free(directory);
	/* this resets last dir returned */
	/*directory_get_path(NULL); */
}

void
directory_delete(struct directory *directory)
{
	assert(holding_db_lock());
	assert(directory != NULL);
	assert(directory->parent != NULL);

	list_del(&directory->siblings);
	directory_free(directory);
}

const char *
directory_get_name(const struct directory *directory)
{
	assert(!directory_is_root(directory));
	assert(directory->path != NULL);

	const char *slash = strrchr(directory->path, '/');
	assert((slash == NULL) == directory_is_root(directory->parent));

	return slash != NULL
		? slash + 1
		: directory->path;
}

struct directory *
directory_new_child(struct directory *parent, const char *name_utf8)
{
	assert(holding_db_lock());
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

	list_add_tail(&directory->siblings, &parent->children);
	return directory;
}

struct directory *
directory_get_child(const struct directory *directory, const char *name)
{
	assert(holding_db_lock());

	struct directory *child;
	directory_for_each_child(child, directory)
		if (strcmp(directory_get_name(child), name) == 0)
			return child;

	return NULL;
}

void
directory_prune_empty(struct directory *directory)
{
	assert(holding_db_lock());

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
	assert(holding_db_lock());
	assert(uri != NULL);

	if (isRootDirectory(uri))
		return directory;

	char *duplicated = g_strdup(uri), *name = duplicated;

	while (1) {
		char *slash = strchr(name, '/');
		if (slash == name) {
			directory = NULL;
			break;
		}

		if (slash != NULL)
			*slash = '\0';

		directory = directory_get_child(directory, name);
		if (directory == NULL || slash == NULL)
			break;

		name = slash + 1;
	}

	g_free(duplicated);

	return directory;
}

void
directory_add_song(struct directory *directory, struct song *song)
{
	assert(holding_db_lock());
	assert(directory != NULL);
	assert(song != NULL);
	assert(song->parent == directory);

	list_add_tail(&song->siblings, &directory->songs);
}

void
directory_remove_song(G_GNUC_UNUSED struct directory *directory,
		      struct song *song)
{
	assert(holding_db_lock());
	assert(directory != NULL);
	assert(song != NULL);
	assert(song->parent == directory);

	list_del(&song->siblings);
}

struct song *
directory_get_song(const struct directory *directory, const char *name_utf8)
{
	assert(holding_db_lock());
	assert(directory != NULL);
	assert(name_utf8 != NULL);

	struct song *song;
	directory_for_each_song(song, directory) {
		assert(song->parent == directory);

		if (strcmp(song->uri, name_utf8) == 0)
			return song;
	}

	return NULL;
}

struct song *
directory_lookup_song(struct directory *directory, const char *uri)
{
	char *duplicated, *base;

	assert(holding_db_lock());
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

	struct song *song = directory_get_song(directory, base);
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
	assert(holding_db_lock());

	list_sort(NULL, &directory->children, directory_cmp);
	song_list_sort(&directory->songs);

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
		struct song *song;
		directory_for_each_song(song, directory)
			if (!visitor->song(song, ctx, error_r))
				return false;
	}

	if (visitor->playlist != NULL) {
		struct playlist_metadata *i;
		directory_for_each_playlist(i, directory)
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
