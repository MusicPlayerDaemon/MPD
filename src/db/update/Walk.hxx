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

#ifndef MPD_UPDATE_WALK_HXX
#define MPD_UPDATE_WALK_HXX

#include "Config.hxx"
#include "Editor.hxx"
#include "util/Compiler.h"
#include "config.h"

#include <atomic>

struct StorageFileInfo;
struct Directory;
struct ArchivePlugin;
struct PlaylistPlugin;
class ArchiveFile;
class Storage;
class ExcludeList;

class UpdateWalk final {
#ifdef ENABLE_ARCHIVE
	friend class UpdateArchiveVisitor;
#endif

	const UpdateConfig config;

	bool walk_discard;
	bool modified;

	/**
	 * Set to true by the main thread when the update thread shall
	 * cancel as quickly as possible.  Access to this flag is
	 * unprotected.
	 */
	std::atomic_bool cancel;

	Storage &storage;

	DatabaseEditor editor;

public:
	UpdateWalk(const UpdateConfig &_config,
		   EventLoop &_loop, DatabaseListener &_listener,
		   Storage &_storage) noexcept;

	/**
	 * Cancel the current update and quit the Walk() method as
	 * soon as possible.
	 */
	void Cancel() noexcept {
		cancel = true;
	}

	/**
	 * Returns true if the database was modified.
	 */
	bool Walk(Directory &root, const char *path, bool discard) noexcept;

private:
	gcc_pure
	bool SkipSymlink(const Directory *directory,
			 const char *utf8_name) const noexcept;

	void RemoveExcludedFromDirectory(Directory &directory,
					 const ExcludeList &exclude_list) noexcept;

	void PurgeDeletedFromDirectory(Directory &directory) noexcept;

	void UpdateSongFile2(Directory &directory,
			     const char *name, const char *suffix,
			     const StorageFileInfo &info) noexcept;

	bool UpdateSongFile(Directory &directory,
			    const char *name, const char *suffix,
			    const StorageFileInfo &info) noexcept;

	bool UpdateContainerFile(Directory &directory,
				 const char *name, const char *suffix,
				 const StorageFileInfo &info) noexcept;


#ifdef ENABLE_ARCHIVE
	void UpdateArchiveTree(ArchiveFile &archive, Directory &parent,
			       const char *name) noexcept;

	bool UpdateArchiveFile(Directory &directory,
			       const char *name, const char *suffix,
			       const StorageFileInfo &info) noexcept;

	void UpdateArchiveFile(Directory &directory, const char *name,
			       const StorageFileInfo &info,
			       const ArchivePlugin &plugin) noexcept;


#else
	bool UpdateArchiveFile(gcc_unused Directory &directory,
			       gcc_unused const char *name,
			       gcc_unused const char *suffix,
			       gcc_unused const StorageFileInfo &info) noexcept {
		return false;
	}
#endif

	void UpdatePlaylistFile(Directory &parent, const char *name,
				const StorageFileInfo &info,
				const PlaylistPlugin &plugin) noexcept;

	bool UpdatePlaylistFile(Directory &directory,
				const char *name, const char *suffix,
				const StorageFileInfo &info) noexcept;

	bool UpdateRegularFile(Directory &directory,
			       const char *name, const StorageFileInfo &info) noexcept;

	void UpdateDirectoryChild(Directory &directory,
				  const ExcludeList &exclude_list,
				  const char *name,
				  const StorageFileInfo &info) noexcept;

	bool UpdateDirectory(Directory &directory,
			     const ExcludeList &exclude_list,
			     const StorageFileInfo &info) noexcept;

	/**
	 * Create the specified directory object if it does not exist
	 * already or if the #StorageFileInfo object indicates that it has been
	 * modified since the last update.  Returns nullptr when it
	 * exists already and is unmodified.
	 *
	 * The caller must lock the database.
	 *
	 * @param virtual_device one of the DEVICE_* constants
	 * specifying the kind of virtual directory
	 */
	Directory *MakeVirtualDirectoryIfModified(Directory &parent,
						  const char *name,
						  const StorageFileInfo &info,
						  unsigned virtual_device) noexcept;

	Directory *LockMakeVirtualDirectoryIfModified(Directory &parent,
						      const char *name,
						      const StorageFileInfo &info,
						      unsigned virtual_device) noexcept;

	Directory *DirectoryMakeChildChecked(Directory &parent,
					     const char *uri_utf8,
					     const char *name_utf8) noexcept;

	Directory *DirectoryMakeUriParentChecked(Directory &root,
						 const char *uri) noexcept;

	void UpdateUri(Directory &root, const char *uri) noexcept;
};

#endif
