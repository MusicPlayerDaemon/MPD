// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
#include "util/SortList.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"

#include <cassert>

#include <string.h>
#include <stdlib.h>

using std::string_view_literals::operator""sv;

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

std::string_view
Directory::GetName() const noexcept
{
	assert(!IsRoot());

	if (parent->IsRoot())
		return path;

	assert(path.starts_with(parent->path));
	assert(path[parent->path.length()] == PathTraitsUTF8::SEPARATOR);

	/* strip the parent directory path and the slash separator
	   from this directory's path, and the base name remains */
	return std::string_view{path}.substr(parent->path.length() + 1);
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
		if (child.GetName() == name)
			return &child;

	return nullptr;
}

Song *
Directory::LookupTargetSong(std::string_view target) noexcept
{
	if (SkipPrefix(target, "../"sv)) {
		if (parent == nullptr)
			return nullptr;

		return parent->LookupTargetSong(target);
	}

	/* sorry for the const_cast ... */
	const auto lr = LookupDirectory(target);
	return lr.directory->FindSong(lr.rest);
}

void
Directory::ClearInPlaylist() noexcept
{
	assert(holding_db_lock());

	for (auto &child : children)
		child.ClearInPlaylist();

	for (auto &song : songs)
		song.in_playlist = false;
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

	auto uri = _uri;

	Directory *d = this;
	do {
		auto [name, rest] = Split(uri, PathTraitsUTF8::SEPARATOR);
		if (name.empty())
			break;

		Directory *tmp = d->FindChild(name);
		if (tmp == nullptr)
			/* not found */
			break;

		d = tmp;

		uri = rest;
	} while (uri.data() != nullptr);

	return { d, _uri.substr(0, uri.data() - _uri.data()), uri };
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

[[gnu::pure]]
static bool
directory_cmp(const Directory &a, const Directory &b) noexcept
{
	return IcuCollate(a.path, b.path) < 0;
}

void
Directory::Sort() noexcept
{
	assert(holding_db_lock());

	SortList(children, directory_cmp);
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
