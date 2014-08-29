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

#ifndef MPD_UPDATE_WALK_HXX
#define MPD_UPDATE_WALK_HXX

#include "check.h"
#include "Editor.hxx"
#include "Compiler.h"

#include <sys/stat.h>

struct stat;
struct FileInfo;
struct Directory;
struct ArchivePlugin;
class Storage;
class ExcludeList;

class UpdateWalk final {
#ifdef ENABLE_ARCHIVE
	friend class UpdateArchiveVisitor;
#endif

#ifndef WIN32
	static constexpr bool DEFAULT_FOLLOW_INSIDE_SYMLINKS = true;
	static constexpr bool DEFAULT_FOLLOW_OUTSIDE_SYMLINKS = true;

	bool follow_inside_symlinks;
	bool follow_outside_symlinks;
#endif

	bool walk_discard;
	bool modified;

	/**
	 * Set to true by the main thread when the update thread shall
	 * cancel as quickly as possible.  Access to this flag is
	 * unprotected.
	 */
	volatile bool cancel;

	Storage &storage;

	DatabaseEditor editor;

public:
	UpdateWalk(EventLoop &_loop, DatabaseListener &_listener,
		   Storage &_storage);

	/**
	 * Cancel the current update and quit the Walk() method as
	 * soon as possible.
	 */
	void Cancel() {
		cancel = true;
	}

	/**
	 * Returns true if the database was modified.
	 */
	bool Walk(Directory &root, const char *path, bool discard);

private:
	gcc_pure
	bool SkipSymlink(const Directory *directory,
			 const char *utf8_name) const;

	void RemoveExcludedFromDirectory(Directory &directory,
					 const ExcludeList &exclude_list);

	void PurgeDeletedFromDirectory(Directory &directory);

	void UpdateSongFile2(Directory &directory,
			     const char *name, const char *suffix,
			     const FileInfo &info);

	bool UpdateSongFile(Directory &directory,
			    const char *name, const char *suffix,
			    const FileInfo &info);

	bool UpdateContainerFile(Directory &directory,
				 const char *name, const char *suffix,
				 const FileInfo &info);


#ifdef ENABLE_ARCHIVE
	void UpdateArchiveTree(Directory &parent, const char *name);

	bool UpdateArchiveFile(Directory &directory,
			       const char *name, const char *suffix,
			       const FileInfo &info);

	void UpdateArchiveFile(Directory &directory, const char *name,
			       const FileInfo &info,
			       const ArchivePlugin &plugin);


#else
	bool UpdateArchiveFile(gcc_unused Directory &directory,
			       gcc_unused const char *name,
			       gcc_unused const char *suffix,
			       gcc_unused const FileInfo &info) {
		return false;
	}
#endif

	bool UpdatePlaylistFile(Directory &directory,
				const char *name, const char *suffix,
				const FileInfo &info);

	bool UpdateRegularFile(Directory &directory,
			       const char *name, const FileInfo &info);

	void UpdateDirectoryChild(Directory &directory,
				  const char *name, const FileInfo &info);

	bool UpdateDirectory(Directory &directory, const FileInfo &info);

	/**
	 * Create the specified directory object if it does not exist
	 * already or if the #stat object indicates that it has been
	 * modified since the last update.  Returns nullptr when it
	 * exists already and is unmodified.
	 *
	 * The caller must lock the database.
	 */
	Directory *MakeDirectoryIfModified(Directory &parent, const char *name,
					   const FileInfo &info);

	Directory *DirectoryMakeChildChecked(Directory &parent,
					     const char *uri_utf8,
					     const char *name_utf8);

	Directory *DirectoryMakeUriParentChecked(Directory &root,
						 const char *uri);

	void UpdateUri(Directory &root, const char *uri);
};

#endif
