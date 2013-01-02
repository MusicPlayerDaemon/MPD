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
#include "SongFilter.hxx"
#include "PlaylistVector.hxx"

extern "C" {
#include "song.h"
#include "song_sort.h"
#include "path.h"
#include "util/list_sort.h"
#include "db_lock.h"
}

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static directory *
directory_allocate(const char *path)
{
	assert(path != NULL);

	const size_t path_size = strlen(path) + 1;
	directory *directory =
		(struct directory *)g_malloc0(sizeof(*directory)
					      - sizeof(directory->path)
					      + path_size);
	INIT_LIST_HEAD(&directory->children);
	INIT_LIST_HEAD(&directory->songs);
	INIT_LIST_HEAD(&directory->playlists);

	memcpy(directory->path, path, path_size);

	return directory;
}

struct directory *
directory_new(const char *path, struct directory *parent)
{
	assert(path != NULL);
	assert((*path == 0) == (parent == NULL));

	directory *directory = directory_allocate(path);

	directory->parent = parent;

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
directory::Walk(bool recursive, const SongFilter *filter,
		VisitDirectory visit_directory, VisitSong visit_song,
		VisitPlaylist visit_playlist,
		GError **error_r) const
{
	assert(error_r == NULL || *error_r == NULL);

	if (visit_song) {
		struct song *song;
		directory_for_each_song(song, this)
			if ((filter == nullptr || filter->Match(*song)) &&
			    !visit_song(*song, error_r))
				return false;
	}

	if (visit_playlist) {
		struct playlist_metadata *i;
		directory_for_each_playlist(i, this)
			if (!visit_playlist(*i, *this, error_r))
				return false;
	}

	struct directory *child;
	directory_for_each_child(child, this) {
		if (visit_directory &&
		    !visit_directory(*child, error_r))
			return false;

		if (recursive &&
		    !child->Walk(recursive, filter,
				 visit_directory, visit_song, visit_playlist,
				 error_r))
			return false;
	}

	return true;
}
