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
#include "db/Directory.hxx"
#include "db/Song.hxx"
#include "db/PlaylistVector.hxx"
#include "db/Uri.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "Mapper.hxx"
#include "ExcludeList.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/DirectoryReader.hxx"
#include "util/Alloc.hxx"
#include "util/UriUtil.hxx"
#include "Log.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

UpdateWalk::UpdateWalk(EventLoop &_loop, DatabaseListener &_listener)
	:editor(_loop, _listener)
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
directory_set_stat(Directory &dir, const struct stat *st)
{
	dir.inode = st->st_ino;
	dir.device = st->st_dev;
	dir.have_stat = true;
}

inline void
UpdateWalk::RemoveExcludedFromDirectory(Directory &directory,
					const ExcludeList &exclude_list)
{
	db_lock();

	Directory *child, *n;
	directory_for_each_child_safe(child, n, directory) {
		const auto name_fs = AllocatedPath::FromUTF8(child->GetName());

		if (name_fs.IsNull() || exclude_list.Check(name_fs)) {
			editor.DeleteDirectory(child);
			modified = true;
		}
	}

	Song *song, *ns;
	directory_for_each_song_safe(song, ns, directory) {
		assert(song->parent == &directory);

		const auto name_fs = AllocatedPath::FromUTF8(song->uri);
		if (name_fs.IsNull() || exclude_list.Check(name_fs)) {
			editor.DeleteSong(directory, song);
			modified = true;
		}
	}

	db_unlock();
}

inline void
UpdateWalk::PurgeDeletedFromDirectory(Directory &directory)
{
	Directory *child, *n;
	directory_for_each_child_safe(child, n, directory) {
		if (directory_exists(*child))
			continue;

		editor.LockDeleteDirectory(child);

		modified = true;
	}

	Song *song, *ns;
	directory_for_each_song_safe(song, ns, directory) {
		const auto path = map_song_fs(*song);
		if (path.IsNull() || !FileExists(path)) {
			editor.LockDeleteSong(directory, song);

			modified = true;
		}
	}

	for (auto i = directory.playlists.begin(),
		     end = directory.playlists.end();
	     i != end;) {
		if (!directory_child_is_regular(directory, i->name.c_str())) {
			db_lock();
			i = directory.playlists.erase(i);
			db_unlock();
		} else
			++i;
	}
}

#ifndef WIN32
static int
update_directory_stat(Directory &directory)
{
	struct stat st;
	if (stat_directory(directory, &st) < 0)
		return -1;

	directory_set_stat(directory, &st);
	return 0;
}
#endif

static int
find_inode_ancestor(Directory *parent, ino_t inode, dev_t device)
{
#ifndef WIN32
	while (parent) {
		if (!parent->have_stat && update_directory_stat(*parent) < 0)
			return -1;

		if (parent->inode == inode && parent->device == device) {
			LogDebug(update_domain, "recursive directory found");
			return 1;
		}

		parent = parent->parent;
	}
#else
	(void)parent;
	(void)inode;
	(void)device;
#endif

	return 0;
}

inline bool
UpdateWalk::UpdatePlaylistFile(Directory &directory,
			       const char *name, const char *suffix,
			       const struct stat *st)
{
	if (!playlist_suffix_supported(suffix))
		return false;

	PlaylistInfo pi(name, st->st_mtime);

	db_lock();
	if (directory.playlists.UpdateOrInsert(std::move(pi)))
		modified = true;
	db_unlock();
	return true;
}

inline bool
UpdateWalk::UpdateRegularFile(Directory &directory,
			      const char *name, const struct stat *st)
{
	const char *suffix = uri_get_suffix(name);
	if (suffix == nullptr)
		return false;

	return UpdateSongFile(directory, name, suffix, st) ||
		UpdateArchiveFile(directory, name, suffix, st) ||
		UpdatePlaylistFile(directory, name, suffix, st);
}

void
UpdateWalk::UpdateDirectoryChild(Directory &directory,
				 const char *name, const struct stat *st)
{
	assert(strchr(name, '/') == nullptr);

	if (S_ISREG(st->st_mode)) {
		UpdateRegularFile(directory, name, st);
	} else if (S_ISDIR(st->st_mode)) {
		if (find_inode_ancestor(&directory, st->st_ino, st->st_dev))
			return;

		db_lock();
		Directory *subdir = directory.MakeChild(name);
		db_unlock();

		assert(&directory == subdir->parent);

		if (!UpdateDirectory(*subdir, st))
			editor.LockDeleteDirectory(subdir);
	} else {
		FormatDebug(update_domain,
			    "%s is not a directory, archive or music", name);
	}
}

/* we don't look at "." / ".." nor files with newlines in their name */
gcc_pure
static bool skip_path(Path path_fs)
{
	const char *path = path_fs.c_str();
	return (path[0] == '.' && path[1] == 0) ||
		(path[0] == '.' && path[1] == '.' && path[2] == 0) ||
		strchr(path, '\n') != nullptr;
}

gcc_pure
bool
UpdateWalk::SkipSymlink(const Directory *directory,
			const char *utf8_name) const
{
#ifndef WIN32
	const auto path_fs = map_directory_child_fs(*directory, utf8_name);
	if (path_fs.IsNull())
		return true;

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
		const char *relative = map_to_relative_path(target_str);
		return relative > target_str
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
UpdateWalk::UpdateDirectory(Directory &directory, const struct stat *st)
{
	assert(S_ISDIR(st->st_mode));

	directory_set_stat(directory, st);

	const auto path_fs = map_directory_fs(directory);
	if (path_fs.IsNull())
		return false;

	DirectoryReader reader(path_fs);
	if (reader.HasFailed()) {
		int error = errno;
		const auto path_utf8 = path_fs.ToUTF8();
		FormatErrno(update_domain, error,
			    "Failed to open directory %s",
			    path_utf8.c_str());
		return false;
	}

	ExcludeList exclude_list;
	exclude_list.LoadFile(AllocatedPath::Build(path_fs, ".mpdignore"));

	if (!exclude_list.IsEmpty())
		RemoveExcludedFromDirectory(directory, exclude_list);

	PurgeDeletedFromDirectory(directory);

	while (reader.ReadEntry()) {
		const auto entry = reader.GetEntry();

		if (skip_path(entry) || exclude_list.Check(entry))
			continue;

		const std::string utf8 = entry.ToUTF8();
		if (utf8.empty())
			continue;

		if (SkipSymlink(&directory, utf8.c_str())) {
			modified |= editor.DeleteNameIn(directory, utf8.c_str());
			continue;
		}

		struct stat st2;
		if (stat_directory_child(directory, utf8.c_str(), &st2) == 0)
			UpdateDirectoryChild(directory, utf8.c_str(), &st2);
		else
			modified |= editor.DeleteNameIn(directory, utf8.c_str());
	}

	directory.mtime = st->st_mtime;

	return true;
}

inline Directory *
UpdateWalk::DirectoryMakeChildChecked(Directory &parent, const char *name_utf8)
{
	db_lock();
	Directory *directory = parent.FindChild(name_utf8);
	db_unlock();

	if (directory != nullptr)
		return directory;

	struct stat st;
	if (stat_directory_child(parent, name_utf8, &st) < 0 ||
	    find_inode_ancestor(&parent, st.st_ino, st.st_dev))
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

	directory_set_stat(*directory, &st);
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

	struct stat st;
	if (!SkipSymlink(parent, name) &&
	    stat_directory_child(*parent, name, &st) == 0)
		UpdateDirectoryChild(*parent, name, &st);
	else
		modified |= editor.DeleteNameIn(*parent, name);
}

bool
UpdateWalk::Walk(Directory &root, const char *path, bool discard)
{
	walk_discard = discard;
	modified = false;

	if (path != nullptr && !isRootDirectory(path)) {
		UpdateUri(root, path);
	} else {
		struct stat st;

		if (stat_directory(root, &st) == 0)
			UpdateDirectory(root, &st);
	}

	return modified;
}
