/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Directory.hxx"
#include "ExportedSong.hxx"
#include "SongSort.hxx"
#include "Song.hxx"
#include "Mount.hxx"
#include "db/LightDirectory.hxx"
#include "db/Uri.hxx"
#include "db/DatabaseLock.hxx"
#include "db/Interface.hxx"
#include "db/Selection.hxx"
#include "song/Filter.hxx"
#include "lib/icu/Collate.hxx"
#include "fs/Traits.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/StringCompare.hxx"
#include "util/StringView.hxx"

#include <cassert>

#include <string.h>
#include <stdlib.h>

Directory::Directory(std::string &&_path_utf8, Directory *_parent) noexcept
	:parent(_parent),
	 path(std::move(_path_utf8))
{
}

Directory::~Directory() noexcept
{
	if (mounted_database != nullptr) {
		mounted_database->Close();
		mounted_database.reset();
	}

	songs.clear_and_dispose(DeleteDisposer());
	children.clear_and_dispose(DeleteDisposer());
}

void
Directory::Delete() noexcept
{
	assert(holding_db_lock());
	assert(parent != nullptr);

	parent->children.erase_and_dispose(parent->children.iterator_to(*this),
					   DeleteDisposer());
}

const char *
Directory::GetName() const noexcept
{
	assert(!IsRoot());

	if (parent->IsRoot())
		return path.c_str();

	assert(StringAfterPrefix(path.c_str(), parent->path.c_str()) != nullptr);
	assert(*StringAfterPrefix(path.c_str(), parent->path.c_str()) == PathTraitsUTF8::SEPARATOR);

	/* strip the parent directory path and the slash separator
	   from this directory's path, and the base name remains */
	return path.c_str() + parent->path.length() + 1;
}

Directory *
Directory::CreateChild(std::string_view name_utf8) noexcept
{
	assert(holding_db_lock());
	assert(!name_utf8.empty());

	std::string path_utf8 = IsRoot()
		? std::string(name_utf8)
		: PathTraitsUTF8::Build(GetPath(), name_utf8);

	auto *child = new Directory(std::move(path_utf8), this);
	children.push_back(*child);
	return child;
}

const Directory *
Directory::FindChild(std::string_view name) const noexcept
{
	assert(holding_db_lock());

	for (const auto &child : children)
		if (name.compare(child.GetName()) == 0)
			return &child;

	return nullptr;
}

Song *
Directory::LookupTargetSong(std::string_view _target) noexcept
{
	StringView target{_target};

	if (target.SkipPrefix("../")) {
		if (parent == nullptr)
			return nullptr;

		return parent->LookupTargetSong(target);
	}

	/* sorry for the const_cast ... */
	const auto lr = LookupDirectory(target);
	return lr.directory->FindSong(lr.rest);
}

void
Directory::PruneEmpty() noexcept
{
	assert(holding_db_lock());

	for (auto child = children.begin(), end = children.end();
	     child != end;) {
		child->PruneEmpty();

		if (child->IsEmpty() && !child->IsMount())
			child = children.erase_and_dispose(child,
							   DeleteDisposer());
		else
			++child;
	}
}

Directory::LookupResult
Directory::LookupDirectory(std::string_view _uri) noexcept
{
	assert(holding_db_lock());

	if (isRootDirectory(_uri))
		return { this, _uri, {} };

	StringView uri(_uri);

	Directory *d = this;
	do {
		auto [name, rest] = uri.Split(PathTraitsUTF8::SEPARATOR);
		if (name.empty())
			break;

		Directory *tmp = d->FindChild(name);
		if (tmp == nullptr)
			/* not found */
			break;

		d = tmp;

		uri = rest;
	} while (uri != nullptr);

	return { d, _uri.substr(0, uri.data - _uri.data()), uri };
}

void
Directory::AddSong(SongPtr song) noexcept
{
	assert(holding_db_lock());
	assert(song != nullptr);
	assert(&song->parent == this);

	songs.push_back(*song.release());
}

SongPtr
Directory::RemoveSong(Song *song) noexcept
{
	assert(holding_db_lock());
	assert(song != nullptr);
	assert(&song->parent == this);

	songs.erase(songs.iterator_to(*song));
	return SongPtr(song);
}

const Song *
Directory::FindSong(std::string_view name_utf8) const noexcept
{
	assert(holding_db_lock());

	for (auto &song : songs) {
		assert(&song.parent == this);

		if (song.filename == name_utf8)
			return &song;
	}

	return nullptr;
}

gcc_pure
static bool
directory_cmp(const Directory &a, const Directory &b) noexcept
{
	return IcuCollate(a.path, b.path) < 0;
}

void
Directory::Sort() noexcept
{
	assert(holding_db_lock());

	children.sort(directory_cmp);
	song_list_sort(songs);

	for (auto &child : children)
		child.Sort();
}

void
Directory::Walk(bool recursive, const SongFilter *filter,
		bool hide_playlist_targets,
		const VisitDirectory& visit_directory, const VisitSong& visit_song,
		const VisitPlaylist& visit_playlist) const
{
	if (IsMount()) {
		assert(IsEmpty());

		/* TODO: eliminate this unlock/lock; it is necessary
		   because the child's SimpleDatabasePlugin::Visit()
		   call will lock it again */
		const ScopeDatabaseUnlock unlock;
		WalkMount(GetPath(), *mounted_database,
			  "", DatabaseSelection("", recursive, filter),
			  visit_directory, visit_song,
			  visit_playlist);
		return;
	}

	if (visit_song) {
		for (auto &song : songs) {
			if (hide_playlist_targets && song.in_playlist)
				continue;

			const auto song2 = song.Export();
			if (filter == nullptr || filter->Match(song2))
				visit_song(song2);
		}
	}

	if (visit_playlist) {
		for (const PlaylistInfo &p : playlists)
			visit_playlist(p, Export());
	}

	for (auto &child : children) {
		if (visit_directory)
			visit_directory(child.Export());

		if (recursive)
			child.Walk(recursive, filter,
				   hide_playlist_targets,
				   visit_directory, visit_song,
				   visit_playlist);
	}
}

LightDirectory
Directory::Export() const noexcept
{
	return {GetPath(), mtime};
}
