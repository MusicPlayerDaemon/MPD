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
#include "Directory.hxx"
#include "SongFilter.hxx"
#include "PlaylistVector.hxx"
#include "DatabaseLock.hxx"

extern "C" {
#include "song.h"
#include "song_sort.h"
#include "util/list_sort.h"
}

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

inline Directory *
Directory::Allocate(const char *path)
{
	assert(path != NULL);

	const size_t path_size = strlen(path) + 1;
	Directory *directory =
		(Directory *)g_malloc0(sizeof(*directory)
				       - sizeof(directory->path)
				       + path_size);
	new(directory) Directory(path);

	return directory;
}

Directory::Directory()
{
	INIT_LIST_HEAD(&children);
	INIT_LIST_HEAD(&songs);

	path[0] = 0;
}

Directory::Directory(const char *_path)
{
	INIT_LIST_HEAD(&children);
	INIT_LIST_HEAD(&songs);

	strcpy(path, _path);
}

Directory::~Directory()
{
	struct song *song, *ns;
	directory_for_each_song_safe(song, ns, this)
		song_free(song);

	Directory *child, *n;
	directory_for_each_child_safe(child, n, this)
		child->Free();
}

Directory *
Directory::NewGeneric(const char *path, Directory *parent)
{
	assert(path != NULL);
	assert((*path == 0) == (parent == NULL));

	Directory *directory = Allocate(path);

	directory->parent = parent;

	return directory;
}

void
Directory::Free()
{
	this->Directory::~Directory();
	g_free(this);
}

void
Directory::Delete()
{
	assert(holding_db_lock());
	assert(parent != nullptr);

	list_del(&siblings);
	Free();
}

const char *
Directory::GetName() const
{
	assert(!IsRoot());
	assert(path != nullptr);

	const char *slash = strrchr(path, '/');
	assert((slash == nullptr) == parent->IsRoot());

	return slash != NULL
		? slash + 1
		: path;
}

Directory *
Directory::CreateChild(const char *name_utf8)
{
	assert(holding_db_lock());
	assert(name_utf8 != NULL);
	assert(*name_utf8 != 0);

	char *allocated;
	const char *path_utf8;
	if (IsRoot()) {
		allocated = NULL;
		path_utf8 = name_utf8;
	} else {
		allocated = g_strconcat(GetPath(),
					"/", name_utf8, NULL);
		path_utf8 = allocated;
	}

	Directory *child = NewGeneric(path_utf8, this);
	g_free(allocated);

	list_add_tail(&child->siblings, &children);
	return child;
}

const Directory *
Directory::FindChild(const char *name) const
{
	assert(holding_db_lock());

	const Directory *child;
	directory_for_each_child(child, this)
		if (strcmp(child->GetName(), name) == 0)
			return child;

	return NULL;
}

void
Directory::PruneEmpty()
{
	assert(holding_db_lock());

	Directory *child, *n;
	directory_for_each_child_safe(child, n, this) {
		child->PruneEmpty();

		if (child->IsEmpty())
			child->Delete();
	}
}

Directory *
Directory::LookupDirectory(const char *uri)
{
	assert(holding_db_lock());
	assert(uri != NULL);

	if (isRootDirectory(uri))
		return this;

	char *duplicated = g_strdup(uri), *name = duplicated;

	Directory *d = this;
	while (1) {
		char *slash = strchr(name, '/');
		if (slash == name) {
			d = NULL;
			break;
		}

		if (slash != NULL)
			*slash = '\0';

		d = d->FindChild(name);
		if (d == NULL || slash == NULL)
			break;

		name = slash + 1;
	}

	g_free(duplicated);

	return d;
}

void
Directory::AddSong(struct song *song)
{
	assert(holding_db_lock());
	assert(song != NULL);
	assert(song->parent == this);

	list_add_tail(&song->siblings, &songs);
}

void
Directory::RemoveSong(struct song *song)
{
	assert(holding_db_lock());
	assert(song != NULL);
	assert(song->parent == this);

	list_del(&song->siblings);
}

const song *
Directory::FindSong(const char *name_utf8) const
{
	assert(holding_db_lock());
	assert(name_utf8 != NULL);

	struct song *song;
	directory_for_each_song(song, this) {
		assert(song->parent == this);

		if (strcmp(song->uri, name_utf8) == 0)
			return song;
	}

	return NULL;
}

struct song *
Directory::LookupSong(const char *uri)
{
	char *duplicated, *base;

	assert(holding_db_lock());
	assert(uri != NULL);

	duplicated = g_strdup(uri);
	base = strrchr(duplicated, '/');

	Directory *d = this;
	if (base != NULL) {
		*base++ = 0;
		d = d->LookupDirectory(duplicated);
		if (d == nullptr) {
			g_free(duplicated);
			return NULL;
		}
	} else
		base = duplicated;

	struct song *song = d->FindSong(base);
	assert(song == NULL || song->parent == d);

	g_free(duplicated);
	return song;

}

static int
directory_cmp(G_GNUC_UNUSED void *priv,
	      struct list_head *_a, struct list_head *_b)
{
	const Directory *a = (const Directory *)_a;
	const Directory *b = (const Directory *)_b;
	return g_utf8_collate(a->path, b->path);
}

void
Directory::Sort()
{
	assert(holding_db_lock());

	list_sort(NULL, &children, directory_cmp);
	song_list_sort(&songs);

	Directory *child;
	directory_for_each_child(child, this)
		child->Sort();
}

bool
Directory::Walk(bool recursive, const SongFilter *filter,
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
		for (const PlaylistInfo &p : playlists)
			if (!visit_playlist(p, *this, error_r))
				return false;
	}

	Directory *child;
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
