/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "LightDirectory.hxx"
#include "Uri.hxx"
#include "SongFilter.hxx"
#include "PlaylistVector.hxx"
#include "db/DatabaseLock.hxx"
#include "SongSort.hxx"
#include "Song.hxx"
#include "LightSong.hxx"
#include "fs/Traits.hxx"
#include "util/Alloc.hxx"
#include "util/Error.hxx"

extern "C" {
#include "util/list_sort.h"
}

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

Directory::Directory(std::string &&_path_utf8, Directory *_parent)
	:parent(_parent),
	 mtime(0), have_stat(false),
	 path(std::move(_path_utf8))
{
	INIT_LIST_HEAD(&children);
	INIT_LIST_HEAD(&songs);
}

Directory::~Directory()
{
	Song *song, *ns;
	directory_for_each_song_safe(song, ns, *this)
		song->Free();

	Directory *child, *n;
	directory_for_each_child_safe(child, n, *this)
		delete child;
}

void
Directory::Delete()
{
	assert(holding_db_lock());
	assert(parent != nullptr);

	list_del(&siblings);
	delete this;
}

const char *
Directory::GetName() const
{
	assert(!IsRoot());

	return PathTraitsUTF8::GetBase(path.c_str());
}

Directory *
Directory::CreateChild(const char *name_utf8)
{
	assert(holding_db_lock());
	assert(name_utf8 != nullptr);
	assert(*name_utf8 != 0);

	std::string path_utf8 = IsRoot()
		? std::string(name_utf8)
		: PathTraitsUTF8::Build(GetPath(), name_utf8);

	Directory *child = new Directory(std::move(path_utf8), this);
	list_add_tail(&child->siblings, &children);
	return child;
}

const Directory *
Directory::FindChild(const char *name) const
{
	assert(holding_db_lock());

	const Directory *child;
	directory_for_each_child(child, *this)
		if (strcmp(child->GetName(), name) == 0)
			return child;

	return nullptr;
}

void
Directory::PruneEmpty()
{
	assert(holding_db_lock());

	Directory *child, *n;
	directory_for_each_child_safe(child, n, *this) {
		child->PruneEmpty();

		if (child->IsEmpty())
			child->Delete();
	}
}

Directory *
Directory::LookupDirectory(const char *uri)
{
	assert(holding_db_lock());
	assert(uri != nullptr);

	if (isRootDirectory(uri))
		return this;

	char *duplicated = xstrdup(uri), *name = duplicated;

	Directory *d = this;
	while (1) {
		char *slash = strchr(name, '/');
		if (slash == name) {
			d = nullptr;
			break;
		}

		if (slash != nullptr)
			*slash = '\0';

		d = d->FindChild(name);
		if (d == nullptr || slash == nullptr)
			break;

		name = slash + 1;
	}

	free(duplicated);

	return d;
}

void
Directory::AddSong(Song *song)
{
	assert(holding_db_lock());
	assert(song != nullptr);
	assert(song->parent == this);

	list_add_tail(&song->siblings, &songs);
}

void
Directory::RemoveSong(Song *song)
{
	assert(holding_db_lock());
	assert(song != nullptr);
	assert(song->parent == this);

	list_del(&song->siblings);
}

const Song *
Directory::FindSong(const char *name_utf8) const
{
	assert(holding_db_lock());
	assert(name_utf8 != nullptr);

	Song *song;
	directory_for_each_song(song, *this) {
		assert(song->parent == this);

		if (strcmp(song->uri, name_utf8) == 0)
			return song;
	}

	return nullptr;
}

Song *
Directory::LookupSong(const char *uri)
{
	char *duplicated, *base;

	assert(holding_db_lock());
	assert(uri != nullptr);

	duplicated = xstrdup(uri);
	base = strrchr(duplicated, '/');

	Directory *d = this;
	if (base != nullptr) {
		*base++ = 0;
		d = d->LookupDirectory(duplicated);
		if (d == nullptr) {
			free(duplicated);
			return nullptr;
		}
	} else
		base = duplicated;

	Song *song = d->FindSong(base);
	assert(song == nullptr || song->parent == d);

	free(duplicated);
	return song;

}

static int
directory_cmp(gcc_unused void *priv,
	      struct list_head *_a, struct list_head *_b)
{
	const Directory *a = (const Directory *)_a;
	const Directory *b = (const Directory *)_b;
	return g_utf8_collate(a->path.c_str(), b->path.c_str());
}

void
Directory::Sort()
{
	assert(holding_db_lock());

	list_sort(nullptr, &children, directory_cmp);
	song_list_sort(&songs);

	Directory *child;
	directory_for_each_child(child, *this)
		child->Sort();
}

bool
Directory::Walk(bool recursive, const SongFilter *filter,
		VisitDirectory visit_directory, VisitSong visit_song,
		VisitPlaylist visit_playlist,
		Error &error) const
{
	assert(!error.IsDefined());

	if (visit_song) {
		Song *song;
		directory_for_each_song(song, *this) {
			const LightSong song2 = song->Export();
			if ((filter == nullptr || filter->Match(song2)) &&
			    !visit_song(song2, error))
				return false;
		}
	}

	if (visit_playlist) {
		for (const PlaylistInfo &p : playlists)
			if (!visit_playlist(p, Export(), error))
				return false;
	}

	Directory *child;
	directory_for_each_child(child, *this) {
		if (visit_directory &&
		    !visit_directory(child->Export(), error))
			return false;

		if (recursive &&
		    !child->Walk(recursive, filter,
				 visit_directory, visit_song, visit_playlist,
				 error))
			return false;
	}

	return true;
}

LightDirectory
Directory::Export() const
{
	return LightDirectory(GetPath(), mtime);
}
