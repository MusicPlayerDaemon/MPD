// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPDATE_WALK_HXX
#define MPD_UPDATE_WALK_HXX

#include "Config.hxx"
#include "Editor.hxx"
#include "config.h"

#include <atomic>
#include <string_view>

struct StorageFileInfo;
struct Directory;
struct ArchivePlugin;
struct PlaylistPlugin;
class SongEnumerator;
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
	[[gnu::pure]]
	bool SkipSymlink(const Directory *directory,
			 std::string_view utf8_name) const noexcept;

	void RemoveExcludedFromDirectory(Directory &directory,
					 const ExcludeList &exclude_list) noexcept;

	void PurgeDeletedFromDirectory(Directory &directory) noexcept;

	/**
	 * Remove all virtual songs inside playlists whose "target"
	 * field points to a non-existing song file.
	 *
	 * It also looks up all target songs and sets their
	 * "in_playlist" field.
	 */
	void PurgeDanglingFromPlaylists(Directory &directory) noexcept;

	void UpdateSongFile2(Directory &directory,
			     std::string_view name, std::string_view suffix,
			     const StorageFileInfo &info) noexcept;

	bool UpdateSongFile(Directory &directory,
			    std::string_view name, std::string_view suffix,
			    const StorageFileInfo &info) noexcept;

	bool UpdateContainerFile(Directory &directory,
				 std::string_view name, std::string_view suffix,
				 const StorageFileInfo &info) noexcept;


#ifdef ENABLE_ARCHIVE
	void UpdateArchiveTree(ArchiveFile &archive, Directory &parent,
			       std::string_view name) noexcept;

	bool UpdateArchiveFile(Directory &directory,
			       std::string_view name, std::string_view suffix,
			       const StorageFileInfo &info) noexcept;

	void UpdateArchiveFile(Directory &directory, std::string_view name,
			       const StorageFileInfo &info,
			       const ArchivePlugin &plugin) noexcept;


#else
	bool UpdateArchiveFile([[maybe_unused]] Directory &directory,
			       [[maybe_unused]] std::string_view name,
			       [[maybe_unused]] std::string_view suffix,
			       [[maybe_unused]] const StorageFileInfo &info) noexcept {
		return false;
	}
#endif

	void UpdatePlaylistFile(Directory &directory,
				SongEnumerator &contents) noexcept;

	void UpdatePlaylistFile(Directory &parent, std::string_view name,
				const StorageFileInfo &info,
				const PlaylistPlugin &plugin) noexcept;

	bool UpdatePlaylistFile(Directory &directory,
				std::string_view name, std::string_view suffix,
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
						  std::string_view name,
						  const StorageFileInfo &info,
						  unsigned virtual_device) noexcept;

	Directory *LockMakeVirtualDirectoryIfModified(Directory &parent,
						      std::string_view name,
						      const StorageFileInfo &info,
						      unsigned virtual_device) noexcept;

	Directory *DirectoryMakeChildChecked(Directory &parent,
					     const char *uri_utf8,
					     std::string_view name_utf8) noexcept;

	Directory *DirectoryMakeUriParentChecked(Directory &root,
						 std::string_view uri) noexcept;

	void UpdateUri(Directory &root, const char *uri) noexcept;
};

#endif
