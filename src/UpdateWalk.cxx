/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "UpdateWalk.hxx"
#include "UpdateIO.hxx"
#include "UpdateDatabase.hxx"
#include "UpdateSong.hxx"
#include "UpdateArchive.hxx"
#include "UpdateDomain.hxx"
#include "DatabaseLock.hxx"
#include "DatabaseSimple.hxx"
#include "Directory.hxx"
#include "Song.hxx"
#include "PlaylistVector.hxx"
#include "PlaylistRegistry.hxx"
#include "Mapper.hxx"
#include "ExcludeList.hxx"
#include "ConfigGlobal.hxx"
#include "ConfigOption.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/DirectoryReader.hxx"
#include "util/UriUtil.hxx"
#include "Log.hxx"

#include <glib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

bool walk_discard;
bool modified;

#ifndef WIN32

enum {
	DEFAULT_FOLLOW_INSIDE_SYMLINKS = true,
	DEFAULT_FOLLOW_OUTSIDE_SYMLINKS = true,
};

static bool follow_inside_symlinks;
static bool follow_outside_symlinks;

#endif

void
update_walk_global_init(void)
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

void
update_walk_global_finish(void)
{
}

static void
directory_set_stat(Directory *dir, const struct stat *st)
{
	dir->inode = st->st_ino;
	dir->device = st->st_dev;
	dir->have_stat = true;
}

static void
remove_excluded_from_directory(Directory *directory,
			       const ExcludeList &exclude_list)
{
	db_lock();

	Directory *child, *n;
	directory_for_each_child_safe(child, n, directory) {
		const auto name_fs = AllocatedPath::FromUTF8(child->GetName());

		if (name_fs.IsNull() || exclude_list.Check(name_fs)) {
			delete_directory(child);
			modified = true;
		}
	}

	Song *song, *ns;
	directory_for_each_song_safe(song, ns, directory) {
		assert(song->parent == directory);

		const auto name_fs = AllocatedPath::FromUTF8(song->uri);
		if (name_fs.IsNull() || exclude_list.Check(name_fs)) {
			delete_song(directory, song);
			modified = true;
		}
	}

	db_unlock();
}

static void
purge_deleted_from_directory(Directory *directory)
{
	Directory *child, *n;
	directory_for_each_child_safe(child, n, directory) {
		if (directory_exists(child))
			continue;

		db_lock();
		delete_directory(child);
		db_unlock();

		modified = true;
	}

	Song *song, *ns;
	directory_for_each_song_safe(song, ns, directory) {
		const auto path = map_song_fs(song);
		if (path.IsNull() || !FileExists(path)) {
			db_lock();
			delete_song(directory, song);
			db_unlock();

			modified = true;
		}
	}

	for (auto i = directory->playlists.begin(),
		     end = directory->playlists.end();
	     i != end;) {
		if (!directory_child_is_regular(directory, i->name.c_str())) {
			db_lock();
			i = directory->playlists.erase(i);
			db_unlock();
		} else
			++i;
	}
}

#ifndef WIN32
static int
update_directory_stat(Directory *directory)
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
		if (!parent->have_stat && update_directory_stat(parent) < 0)
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

static bool
update_playlist_file2(Directory *directory,
		      const char *name, const char *suffix,
		      const struct stat *st)
{
	if (!playlist_suffix_supported(suffix))
		return false;

	PlaylistInfo pi(name, st->st_mtime);

	db_lock();
	if (directory->playlists.UpdateOrInsert(std::move(pi)))
		modified = true;
	db_unlock();
	return true;
}

static bool
update_regular_file(Directory *directory,
		    const char *name, const struct stat *st)
{
	const char *suffix = uri_get_suffix(name);
	if (suffix == nullptr)
		return false;

	return update_song_file(directory, name, suffix, st) ||
		update_archive_file(directory, name, suffix, st) ||
		update_playlist_file2(directory, name, suffix, st);
}

static bool
update_directory(Directory *directory, const struct stat *st);

static void
update_directory_child(Directory *directory,
		       const char *name, const struct stat *st)
{
	assert(strchr(name, '/') == nullptr);

	if (S_ISREG(st->st_mode)) {
		update_regular_file(directory, name, st);
	} else if (S_ISDIR(st->st_mode)) {
		if (find_inode_ancestor(directory, st->st_ino, st->st_dev))
			return;

		db_lock();
		Directory *subdir = directory->MakeChild(name);
		db_unlock();

		assert(directory == subdir->parent);

		if (!update_directory(subdir, st)) {
			db_lock();
			delete_directory(subdir);
			db_unlock();
		}
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
static bool
skip_symlink(const Directory *directory, const char *utf8_name)
{
#ifndef WIN32
	const auto path_fs = map_directory_child_fs(directory, utf8_name);
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

	if (PathTraits::IsAbsoluteFS(target_str)) {
		/* if the symlink points to an absolute path, see if
		   that path is inside the music directory */
		const char *relative = map_to_relative_path(target_str);
		return relative > target_str
			? !follow_inside_symlinks
			: !follow_outside_symlinks;
	}

	const char *p = target_str;
	while (*p == '.') {
		if (p[1] == '.' && PathTraits::IsSeparatorFS(p[2])) {
			/* "../" moves to parent directory */
			directory = directory->parent;
			if (directory == nullptr) {
				/* we have moved outside the music
				   directory - skip this symlink
				   if such symlinks are not allowed */
				return !follow_outside_symlinks;
			}
			p += 3;
		} else if (PathTraits::IsSeparatorFS(p[1]))
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

static bool
update_directory(Directory *directory, const struct stat *st)
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
		remove_excluded_from_directory(directory, exclude_list);

	purge_deleted_from_directory(directory);

	while (reader.ReadEntry()) {
		std::string utf8;
		struct stat st2;

		const auto entry = reader.GetEntry();

		if (skip_path(entry) || exclude_list.Check(entry))
			continue;

		utf8 = entry.ToUTF8();
		if (utf8.empty())
			continue;

		if (skip_symlink(directory, utf8.c_str())) {
			modified |= delete_name_in(directory, utf8.c_str());
			continue;
		}

		if (stat_directory_child(directory, utf8.c_str(), &st2) == 0)
			update_directory_child(directory, utf8.c_str(), &st2);
		else
			modified |= delete_name_in(directory, utf8.c_str());
	}

	directory->mtime = st->st_mtime;

	return true;
}

static Directory *
directory_make_child_checked(Directory *parent, const char *name_utf8)
{
	db_lock();
	Directory *directory = parent->FindChild(name_utf8);
	db_unlock();

	if (directory != nullptr)
		return directory;

	struct stat st;
	if (stat_directory_child(parent, name_utf8, &st) < 0 ||
	    find_inode_ancestor(parent, st.st_ino, st.st_dev))
		return nullptr;

	if (skip_symlink(parent, name_utf8))
		return nullptr;

	/* if we're adding directory paths, make sure to delete filenames
	   with potentially the same name */
	db_lock();
	Song *conflicting = parent->FindSong(name_utf8);
	if (conflicting)
		delete_song(parent, conflicting);

	directory = parent->CreateChild(name_utf8);
	db_unlock();

	directory_set_stat(directory, &st);
	return directory;
}

static Directory *
directory_make_uri_parent_checked(const char *uri)
{
	Directory *directory = db_get_root();
	char *duplicated = g_strdup(uri);
	char *name_utf8 = duplicated, *slash;

	while ((slash = strchr(name_utf8, '/')) != nullptr) {
		*slash = 0;

		if (*name_utf8 == 0)
			continue;

		directory = directory_make_child_checked(directory, name_utf8);
		if (directory == nullptr)
			break;

		name_utf8 = slash + 1;
	}

	g_free(duplicated);
	return directory;
}

static void
update_uri(const char *uri)
{
	Directory *parent = directory_make_uri_parent_checked(uri);
	if (parent == nullptr)
		return;

	char *name = g_path_get_basename(uri);

	struct stat st;
	if (!skip_symlink(parent, name) &&
	    stat_directory_child(parent, name, &st) == 0)
		update_directory_child(parent, name, &st);
	else
		modified |= delete_name_in(parent, name);

	g_free(name);
}

bool
update_walk(const char *path, bool discard)
{
	walk_discard = discard;
	modified = false;

	if (path != nullptr && !isRootDirectory(path)) {
		update_uri(path);
	} else {
		Directory *directory = db_get_root();
		struct stat st;

		if (stat_directory(directory, &st) == 0)
			update_directory(directory, &st);
	}

	return modified;
}
