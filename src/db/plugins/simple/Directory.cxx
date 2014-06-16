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
#include "SongSort.hxx"
#include "Song.hxx"
#include "Mount.hxx"
#include "db/LightDirectory.hxx"
#include "db/LightSong.hxx"
#include "db/Uri.hxx"
#include "db/DatabaseLock.hxx"
#include "db/Interface.hxx"
#include "SongFilter.hxx"
#include "lib/icu/Collate.hxx"
#include "fs/Traits.hxx"
#include "util/Alloc.hxx"
#include "util/Error.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

Directory::Directory(std::string &&_path_utf8, Directory *_parent)
	:parent(_parent),
	 mtime(0),
	 inode(0), device(0),
	 path(std::move(_path_utf8)),
	 mounted_database(nullptr)
{
}

Directory::~Directory()
{
	delete mounted_database;

	songs.clear_and_dispose(Song::Disposer());
	children.clear_and_dispose(Disposer());
}

void
Directory::Delete()
{
	assert(holding_db_lock());
	assert(parent != nullptr);

	parent->children.erase_and_dispose(parent->children.iterator_to(*this),
					   Disposer());
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
	children.push_back(*child);
	return child;
}

const Directory *
Directory::FindChild(const char *name) const
{
	assert(holding_db_lock());

	for (const auto &child : children)
		if (strcmp(child.GetName(), name) == 0)
			return &child;

	return nullptr;
}

void
Directory::PruneEmpty()
{
	assert(holding_db_lock());

	for (auto child = children.begin(), end = children.end();
	     child != end;) {
		child->PruneEmpty();

		if (child->IsEmpty())
			child = children.erase_and_dispose(child, Disposer());
		else
			++child;
	}
}

Directory::LookupResult
Directory::LookupDirectory(const char *uri)
{
	assert(holding_db_lock());
	assert(uri != nullptr);

	if (isRootDirectory(uri))
		return { this, nullptr };

	char *duplicated = xstrdup(uri), *name = duplicated;

	Directory *d = this;
	while (true) {
		char *slash = strchr(name, '/');
		if (slash == name)
			break;

		if (slash != nullptr)
			*slash = '\0';

		Directory *tmp = d->FindChild(name);
		if (tmp == nullptr)
			/* not found */
			break;

		d = tmp;

		if (slash == nullptr) {
			/* found everything */
			name = nullptr;
			break;
		}

		name = slash + 1;
	}

	free(duplicated);

	const char *rest = name == nullptr
		? nullptr
		: uri + (name - duplicated);

	return { d, rest };
}

void
Directory::AddSong(Song *song)
{
	assert(holding_db_lock());
	assert(song != nullptr);
	assert(song->parent == this);

	songs.push_back(*song);
}

void
Directory::RemoveSong(Song *song)
{
	assert(holding_db_lock());
	assert(song != nullptr);
	assert(song->parent == this);

	songs.erase(songs.iterator_to(*song));
}

const Song *
Directory::FindSong(const char *name_utf8) const
{
	assert(holding_db_lock());
	assert(name_utf8 != nullptr);

	for (auto &song : songs) {
		assert(song.parent == this);

		if (strcmp(song.uri, name_utf8) == 0)
			return &song;
	}

	return nullptr;
}

gcc_pure
static bool
directory_cmp(const Directory &a, const Directory &b)
{
	return IcuCollate(a.path.c_str(), b.path.c_str()) < 0;
}

void
Directory::Sort()
{
	assert(holding_db_lock());

	children.sort(directory_cmp);
	song_list_sort(songs);

	for (auto &child : children)
		child.Sort();
}

bool
Directory::Walk(bool recursive, const SongFilter *filter,
		VisitDirectory visit_directory, VisitSong visit_song,
		VisitPlaylist visit_playlist,
		Error &error) const
{
	assert(!error.IsDefined());

	if (IsMount()) {
		assert(IsEmpty());

		/* TODO: eliminate this unlock/lock; it is necessary
		   because the child's SimpleDatabasePlugin::Visit()
		   call will lock it again */
		db_unlock();
		bool result = WalkMount(GetPath(), *mounted_database,
					recursive, filter,
					visit_directory, visit_song,
					visit_playlist,
					error);
		db_lock();
		return result;
	}

	if (visit_song) {
		for (auto &song : songs){
			const LightSong song2 = song.Export();
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

	for (auto &child : children) {
		if (visit_directory &&
		    !visit_directory(child.Export(), error))
			return false;

		if (recursive &&
		    !child.Walk(recursive, filter,
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
