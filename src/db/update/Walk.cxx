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

#include "config.h" /* must be first for large file support */
#include "Walk.hxx"
#include "UpdateIO.hxx"
#include "Editor.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseLock.hxx"
#include "db/PlaylistVector.hxx"
#include "db/Uri.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "db/plugins/simple/Song.hxx"
#include "storage/StorageInterface.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "ExcludeList.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/Charset.hxx"
#include "storage/FileInfo.hxx"
#include "util/Alloc.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <memory>

UpdateWalk::UpdateWalk(EventLoop &_loop, DatabaseListener &_listener,
		       Storage &_storage)
	:cancel(false),
	 storage(_storage),
	 editor(_loop, _listener)
{
#ifndef WIN32
	follow_inside_symlinks =
		config_get_bool(CONF_FOLLOW_INSIDE_SYMLINKS,
				DEFAULT_FOLLOW_INSIDE_SYMLINKS);

	follow_outside_symlinks =
		config_get_bool(CONF_FOLLOW_OUTSIDE_SYMLINKS,
				DEFAULT_FOLLOW_OUTSIDE_SYMLINKS);
#endif
}

static void
directory_set_stat(Directory &dir, const FileInfo &info)
{
	dir.inode = info.inode;
	dir.device = info.device;
}

inline void
UpdateWalk::RemoveExcludedFromDirectory(Directory &directory,
					const ExcludeList &exclude_list)
{
	db_lock();

	directory.ForEachChildSafe([&](Directory &child){
			const auto name_fs =
				AllocatedPath::FromUTF8(child.GetName());

			if (name_fs.IsNull() || exclude_list.Check(name_fs)) {
				editor.DeleteDirectory(&child);
				modified = true;
			}
		});

	directory.ForEachSongSafe([&](Song &song){
			assert(song.parent == &directory);

			const auto name_fs = AllocatedPath::FromUTF8(song.uri);
			if (name_fs.IsNull() || exclude_list.Check(name_fs)) {
				editor.DeleteSong(directory, &song);
				modified = true;
			}
		});

	db_unlock();
}

inline void
UpdateWalk::PurgeDeletedFromDirectory(Directory &directory)
{
	directory.ForEachChildSafe([&](Directory &child){
			if (DirectoryExists(storage, child))
				return;

			editor.LockDeleteDirectory(&child);

			modified = true;
		});

	directory.ForEachSongSafe([&](Song &song){
			if (!directory_child_is_regular(storage, directory,
							song.uri)) {
				editor.LockDeleteSong(directory, &song);

				modified = true;
			}
		});

	for (auto i = directory.playlists.begin(),
		     end = directory.playlists.end();
	     i != end;) {
		if (!directory_child_is_regular(storage, directory,
						i->name.c_str())) {
			db_lock();
			i = directory.playlists.erase(i);
			db_unlock();
		} else
			++i;
	}
}

#ifndef WIN32
static bool
update_directory_stat(Storage &storage, Directory &directory)
{
	FileInfo info;
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
		 unsigned inode, unsigned device)
{
#ifndef WIN32
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
UpdateWalk::UpdatePlaylistFile(Directory &directory,
			       const char *name, const char *suffix,
			       const FileInfo &info)
{
	if (!playlist_suffix_supported(suffix))
		return false;

	PlaylistInfo pi(name, info.mtime);

	db_lock();
	if (directory.playlists.UpdateOrInsert(std::move(pi)))
		modified = true;
	db_unlock();
	return true;
}

inline bool
UpdateWalk::UpdateRegularFile(Directory &directory,
			      const char *name, const FileInfo &info)
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
				 const char *name, const FileInfo &info)
{
	assert(strchr(name, '/') == nullptr);

	if (info.IsRegular()) {
		UpdateRegularFile(directory, name, info);
	} else if (info.IsDirectory()) {
		if (FindAncestorLoop(storage, &directory,
					info.inode, info.device))
			return;

		db_lock();
		Directory *subdir = directory.MakeChild(name);
		db_unlock();

		assert(&directory == subdir->parent);

		if (!UpdateDirectory(*subdir, info))
			editor.LockDeleteDirectory(subdir);
	} else {
		FormatDebug(update_domain,
			    "%s is not a directory, archive or music", name);
	}
}

/* we don't look at "." / ".." nor files with newlines in their name */
gcc_pure
static bool
skip_path(const char *name_utf8)
{
	return strchr(name_utf8, '\n') != nullptr;
}

gcc_pure
bool
UpdateWalk::SkipSymlink(const Directory *directory,
			const char *utf8_name) const
{
#ifndef WIN32
	const auto path_fs = storage.MapChildFS(directory->GetPath(),
						utf8_name);
	if (path_fs.IsNull())
		/* not a local file: don't skip */
		return false;

	const auto target = ReadLink(path_fs);
	if (target.IsNull())
		/* don't skip if this is not a symlink */
		return errno != EINVAL;

	if (!follow_inside_symlinks && !follow_outside_symlinks) {
		/* ignore all symlinks */
		return true;
	} else if (follow_inside_symlinks && follow_outside_symlinks) {
		/* consider all symlinks */
		return false;
	}

	const char *target_str = target.c_str();

	if (PathTraitsFS::IsAbsolute(target_str)) {
		/* if the symlink points to an absolute path, see if
		   that path is inside the music directory */
		const auto target_utf8 = PathToUTF8(target_str);
		if (target_utf8.empty())
			return true;

		const char *relative =
			storage.MapToRelativeUTF8(target_utf8.c_str());
		return relative != nullptr
			? !follow_inside_symlinks
			: !follow_outside_symlinks;
	}

	const char *p = target_str;
	while (*p == '.') {
		if (p[1] == '.' && PathTraitsFS::IsSeparator(p[2])) {
			/* "../" moves to parent directory */
			directory = directory->parent;
			if (directory == nullptr) {
				/* we have moved outside the music
				   directory - skip this symlink
				   if such symlinks are not allowed */
				return !follow_outside_symlinks;
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
	return !follow_inside_symlinks;
#else
	/* no symlink checking on WIN32 */

	(void)directory;
	(void)utf8_name;

	return false;
#endif
}

bool
UpdateWalk::UpdateDirectory(Directory &directory, const FileInfo &info)
{
	assert(info.IsDirectory());

	directory_set_stat(directory, info);

	Error error;
	const std::unique_ptr<StorageDirectoryReader> reader(storage.OpenDirectory(directory.GetPath(), error));
	if (reader.get() == nullptr) {
		LogError(error);
		return false;
	}

	ExcludeList exclude_list;

	{
		const auto exclude_path_fs =
			storage.MapChildFS(directory.GetPath(), ".mpdignore");
		if (!exclude_path_fs.IsNull())
			exclude_list.LoadFile(exclude_path_fs);
	}

	if (!exclude_list.IsEmpty())
		RemoveExcludedFromDirectory(directory, exclude_list);

	PurgeDeletedFromDirectory(directory);

	const char *name_utf8;
	while (!cancel && (name_utf8 = reader->Read()) != nullptr) {
		if (skip_path(name_utf8))
			continue;

		{
			const auto name_fs = AllocatedPath::FromUTF8(name_utf8);
			if (name_fs.IsNull() || exclude_list.Check(name_fs))
				continue;
		}

		if (SkipSymlink(&directory, name_utf8)) {
			modified |= editor.DeleteNameIn(directory, name_utf8);
			continue;
		}

		FileInfo info2;
		if (!GetInfo(*reader, info2)) {
			modified |= editor.DeleteNameIn(directory, name_utf8);
			continue;
		}

		UpdateDirectoryChild(directory, name_utf8, info2);
	}

	directory.mtime = info.mtime;

	return true;
}

inline Directory *
UpdateWalk::DirectoryMakeChildChecked(Directory &parent,
				      const char *uri_utf8,
				      const char *name_utf8)
{
	db_lock();
	Directory *directory = parent.FindChild(name_utf8);
	db_unlock();

	if (directory != nullptr) {
		if (directory->IsMount())
			directory = nullptr;

		return directory;
	}

	FileInfo info;
	if (!GetInfo(storage, uri_utf8, info) ||
	    FindAncestorLoop(storage, &parent, info.inode, info.device))
		return nullptr;

	if (SkipSymlink(&parent, name_utf8))
		return nullptr;

	/* if we're adding directory paths, make sure to delete filenames
	   with potentially the same name */
	db_lock();
	Song *conflicting = parent.FindSong(name_utf8);
	if (conflicting)
		editor.DeleteSong(parent, conflicting);

	directory = parent.CreateChild(name_utf8);
	db_unlock();

	directory_set_stat(*directory, info);
	return directory;
}

inline Directory *
UpdateWalk::DirectoryMakeUriParentChecked(Directory &root, const char *uri)
{
	Directory *directory = &root;
	char *duplicated = xstrdup(uri);
	char *name_utf8 = duplicated, *slash;

	while ((slash = strchr(name_utf8, '/')) != nullptr) {
		*slash = 0;

		if (*name_utf8 == 0)
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
UpdateWalk::UpdateUri(Directory &root, const char *uri)
{
	Directory *parent = DirectoryMakeUriParentChecked(root, uri);
	if (parent == nullptr)
		return;

	const char *name = PathTraitsUTF8::GetBase(uri);

	if (SkipSymlink(parent, name)) {
		modified |= editor.DeleteNameIn(*parent, name);
		return;
	}

	FileInfo info;
	if (!GetInfo(storage, uri, info)) {
		modified |= editor.DeleteNameIn(*parent, name);
		return;
	}

	UpdateDirectoryChild(*parent, name, info);
}

bool
UpdateWalk::Walk(Directory &root, const char *path, bool discard)
{
	walk_discard = discard;
	modified = false;

	if (path != nullptr && !isRootDirectory(path)) {
		UpdateUri(root, path);
	} else {
		FileInfo info;
		if (!GetInfo(storage, "", info))
			return false;

		UpdateDirectory(root, info);
	}

	return modified;
}
