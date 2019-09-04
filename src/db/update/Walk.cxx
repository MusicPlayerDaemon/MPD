/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "Walk.hxx"
#include "UpdateIO.hxx"
#include "Editor.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseLock.hxx"
#include "db/Uri.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "db/plugins/simple/Song.hxx"
#include "storage/StorageInterface.hxx"
#include "ExcludeList.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "storage/FileInfo.hxx"
#include "input/InputStream.hxx"
#include "input/Error.hxx"
#include "util/Alloc.hxx"
#include "util/StringCompare.hxx"
#include "util/UriExtract.hxx"
#include "Log.hxx"

#include <exception>
#include <memory>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

UpdateWalk::UpdateWalk(const UpdateConfig &_config,
		       EventLoop &_loop, DatabaseListener &_listener,
		       Storage &_storage) noexcept
	:config(_config), cancel(false),
	 storage(_storage),
	 editor(_loop, _listener)
{
}

static void
directory_set_stat(Directory &dir, const StorageFileInfo &info)
{
	dir.inode = info.inode;
	dir.device = info.device;
}

inline void
UpdateWalk::RemoveExcludedFromDirectory(Directory &directory,
					const ExcludeList &exclude_list) noexcept
{
	const ScopeDatabaseLock protect;

	directory.ForEachChildSafe([&](Directory &child){
			const auto name_fs =
				AllocatedPath::FromUTF8(child.GetName());

			if (name_fs.IsNull() || exclude_list.Check(name_fs)) {
				editor.DeleteDirectory(&child);
				modified = true;
			}
		});

	directory.ForEachSongSafe([&](Song &song){
			assert(&song.parent == &directory);

			const auto name_fs = AllocatedPath::FromUTF8(song.filename.c_str());
			if (name_fs.IsNull() || exclude_list.Check(name_fs)) {
				editor.DeleteSong(directory, &song);
				modified = true;
			}
		});
}

inline void
UpdateWalk::PurgeDeletedFromDirectory(Directory &directory) noexcept
{
	directory.ForEachChildSafe([&](Directory &child){
			if (child.IsMount() || DirectoryExists(storage, child))
				return;

			editor.LockDeleteDirectory(&child);

			modified = true;
		});

	directory.ForEachSongSafe([&](Song &song){
			if (!directory_child_is_regular(storage, directory,
							song.filename.c_str())) {
				editor.LockDeleteSong(directory, &song);

				modified = true;
			}
		});

	for (auto i = directory.playlists.begin(),
		     end = directory.playlists.end();
	     i != end;) {
		if (!directory_child_is_regular(storage, directory,
						i->name.c_str())) {
			const ScopeDatabaseLock protect;
			i = directory.playlists.erase(i);
		} else
			++i;
	}
}

#ifndef _WIN32
static bool
update_directory_stat(Storage &storage, Directory &directory) noexcept
{
	StorageFileInfo info;
	if (!GetInfo(storage, directory.GetPath(), info))
		return false;

	directory_set_stat(directory, info);
	return true;
}
#endif

/**
 * Check the ancestors of the given #Directory and see if there's one
 * with the same device/inode number, building a loop.
 *
 * @return 1 if a loop was found, 0 if not, -1 on I/O error
 */
static int
FindAncestorLoop(Storage &storage, Directory *parent,
		 unsigned inode, unsigned device) noexcept
{
#ifndef _WIN32
	if (device == 0 && inode == 0)
		/* can't detect loops if the Storage does not support
		   these numbers */
		return 0;

	while (parent) {
		if (parent->device == 0 && parent->inode == 0 &&
		    !update_directory_stat(storage, *parent))
			return -1;

		if (parent->inode == inode && parent->device == device) {
			LogDebug(update_domain, "recursive directory found");
			return 1;
		}

		parent = parent->parent;
	}
#else
	(void)storage;
	(void)parent;
	(void)inode;
	(void)device;
#endif

	return 0;
}

inline bool
UpdateWalk::UpdateRegularFile(Directory &directory,
			      const char *name,
			      const StorageFileInfo &info) noexcept
{
	const char *suffix = uri_get_suffix(name);
	if (suffix == nullptr)
		return false;

	return UpdateSongFile(directory, name, suffix, info) ||
		UpdateArchiveFile(directory, name, suffix, info) ||
		UpdatePlaylistFile(directory, name, suffix, info);
}

void
UpdateWalk::UpdateDirectoryChild(Directory &directory,
				 const ExcludeList &exclude_list,
				 const char *name, const StorageFileInfo &info) noexcept
try {
	assert(strchr(name, '/') == nullptr);

	if (info.IsRegular()) {
		UpdateRegularFile(directory, name, info);
	} else if (info.IsDirectory()) {
		if (FindAncestorLoop(storage, &directory,
					info.inode, info.device))
			return;

		Directory *subdir;
		{
			const ScopeDatabaseLock protect;
			subdir = directory.MakeChild(name);
		}

		assert(&directory == subdir->parent);

		if (!UpdateDirectory(*subdir, exclude_list, info))
			editor.LockDeleteDirectory(subdir);
	} else {
		FormatDebug(update_domain,
			    "%s is not a directory, archive or music", name);
	}
} catch (...) {
	LogError(std::current_exception());
}

/* we don't look at "." / ".." nor files with newlines in their name */
gcc_pure
static bool
skip_path(const char *name_utf8) noexcept
{
	return strchr(name_utf8, '\n') != nullptr;
}

gcc_pure
bool
UpdateWalk::SkipSymlink(const Directory *directory,
			const char *utf8_name) const noexcept
{
#ifndef _WIN32
	const auto path_fs = storage.MapChildFS(directory->GetPath(),
						utf8_name);
	if (path_fs.IsNull())
		/* not a local file: don't skip */
		return false;

	const auto target = ReadLink(path_fs);
	if (target.IsNull())
		/* don't skip if this is not a symlink */
		return errno != EINVAL;

	if (!config.follow_inside_symlinks &&
	    !config.follow_outside_symlinks) {
		/* ignore all symlinks */
		return true;
	} else if (config.follow_inside_symlinks &&
		   config.follow_outside_symlinks) {
		/* consider all symlinks */
		return false;
	}

	if (target.IsAbsolute()) {
		/* if the symlink points to an absolute path, see if
		   that path is inside the music directory */
		const auto target_utf8 = target.ToUTF8();
		if (target_utf8.empty())
			return true;

		const char *relative =
			storage.MapToRelativeUTF8(target_utf8.c_str());
		return relative != nullptr
			? !config.follow_inside_symlinks
			: !config.follow_outside_symlinks;
	}

	const char *p = target.c_str();
	while (*p == '.') {
		if (p[1] == '.' && PathTraitsFS::IsSeparator(p[2])) {
			/* "../" moves to parent directory */
			directory = directory->parent;
			if (directory == nullptr) {
				/* we have moved outside the music
				   directory - skip this symlink
				   if such symlinks are not allowed */
				return !config.follow_outside_symlinks;
			}
			p += 3;
		} else if (PathTraitsFS::IsSeparator(p[1]))
			/* eliminate "./" */
			p += 2;
		else
			break;
	}

	/* we are still in the music directory, so this symlink points
	   to a song which is already in the database - skip according
	   to the follow_inside_symlinks param*/
	return !config.follow_inside_symlinks;
#else
	/* no symlink checking on WIN32 */

	(void)directory;
	(void)utf8_name;

	return false;
#endif
}

bool
UpdateWalk::UpdateDirectory(Directory &directory,
			    const ExcludeList &exclude_list,
			    const StorageFileInfo &info) noexcept
{
	assert(info.IsDirectory());

	directory_set_stat(directory, info);

	std::unique_ptr<StorageDirectoryReader> reader;

	try {
		reader = storage.OpenDirectory(directory.GetPath());
	} catch (...) {
		LogError(std::current_exception());
		return false;
	}

	ExcludeList child_exclude_list(exclude_list);

	try {
		Mutex mutex;
		auto is = InputStream::OpenReady(PathTraitsUTF8::Build(storage.MapUTF8(directory.GetPath()).c_str(),
								       ".mpdignore").c_str(),
						 mutex);
		child_exclude_list.Load(std::move(is));
	} catch (...) {
		if (!IsFileNotFound(std::current_exception()))
			LogError(std::current_exception());
	}

	if (!child_exclude_list.IsEmpty())
		RemoveExcludedFromDirectory(directory, child_exclude_list);

	PurgeDeletedFromDirectory(directory);

	const char *name_utf8;
	while (!cancel && (name_utf8 = reader->Read()) != nullptr) {
		if (skip_path(name_utf8))
			continue;

		{
			const auto name_fs = AllocatedPath::FromUTF8(name_utf8);
			if (name_fs.IsNull() || child_exclude_list.Check(name_fs))
				continue;
		}

		if (SkipSymlink(&directory, name_utf8)) {
			modified |= editor.DeleteNameIn(directory, name_utf8);
			continue;
		}

		StorageFileInfo info2;
		if (!GetInfo(*reader, info2)) {
			modified |= editor.DeleteNameIn(directory, name_utf8);
			continue;
		}

		UpdateDirectoryChild(directory, child_exclude_list, name_utf8, info2);
	}

	directory.mtime = info.mtime;

	return true;
}

inline Directory *
UpdateWalk::DirectoryMakeChildChecked(Directory &parent,
				      const char *uri_utf8,
				      const char *name_utf8) noexcept
{
	Directory *directory;
	{
		const ScopeDatabaseLock protect;
		directory = parent.FindChild(name_utf8);
	}

	if (directory != nullptr) {
		if (directory->IsMount())
			directory = nullptr;

		return directory;
	}

	StorageFileInfo info;
	if (!GetInfo(storage, uri_utf8, info) ||
	    FindAncestorLoop(storage, &parent, info.inode, info.device))
		return nullptr;

	if (SkipSymlink(&parent, name_utf8))
		return nullptr;

	/* if we're adding directory paths, make sure to delete filenames
	   with potentially the same name */
	{
		const ScopeDatabaseLock protect;
		Song *conflicting = parent.FindSong(name_utf8);
		if (conflicting)
			editor.DeleteSong(parent, conflicting);

		directory = parent.CreateChild(name_utf8);
	}

	directory_set_stat(*directory, info);
	return directory;
}

inline Directory *
UpdateWalk::DirectoryMakeUriParentChecked(Directory &root,
					  const char *uri) noexcept
{
	Directory *directory = &root;
	char *duplicated = xstrdup(uri);
	char *name_utf8 = duplicated, *slash;

	while ((slash = strchr(name_utf8, '/')) != nullptr) {
		*slash = 0;

		if (StringIsEmpty(name_utf8))
			continue;

		directory = DirectoryMakeChildChecked(*directory,
						      duplicated,
						      name_utf8);
		if (directory == nullptr)
			break;

		name_utf8 = slash + 1;
	}

	free(duplicated);
	return directory;
}

inline void
UpdateWalk::UpdateUri(Directory &root, const char *uri) noexcept
try {
	Directory *parent = DirectoryMakeUriParentChecked(root, uri);
	if (parent == nullptr)
		return;

	const char *name = PathTraitsUTF8::GetBase(uri);

	if (SkipSymlink(parent, name)) {
		modified |= editor.DeleteNameIn(*parent, name);
		return;
	}

	StorageFileInfo info;
	if (!GetInfo(storage, uri, info)) {
		modified |= editor.DeleteNameIn(*parent, name);
		return;
	}

	ExcludeList exclude_list;

	UpdateDirectoryChild(*parent, exclude_list, name, info);
} catch (...) {
	LogError(std::current_exception());
}

bool
UpdateWalk::Walk(Directory &root, const char *path, bool discard) noexcept
{
	walk_discard = discard;
	modified = false;

	if (path != nullptr && !isRootDirectory(path)) {
		UpdateUri(root, path);
	} else {
		StorageFileInfo info;
		if (!GetInfo(storage, "", info))
			return false;

		ExcludeList exclude_list;

		UpdateDirectory(root, exclude_list, info);
	}

	return modified;
}
