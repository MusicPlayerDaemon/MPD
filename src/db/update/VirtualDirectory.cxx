// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Walk.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "storage/FileInfo.hxx"

Directory *
UpdateWalk::MakeVirtualDirectoryIfModified(Directory &parent, std::string_view name,
					   const StorageFileInfo &info,
					   unsigned virtual_device) noexcept
{
	Directory *directory = parent.FindChild(name);

	// directory exists already
	if (directory != nullptr) {
		if (directory->IsMount())
			return nullptr;

		if (directory->mtime == info.mtime &&
		    directory->device == virtual_device &&
		    !walk_discard) {
			/* not modified */
			return nullptr;
		}

		editor.DeleteDirectory(directory);
		modified = true;
	}

	directory = parent.MakeChild(name);
	directory->mtime = info.mtime;
	directory->device = virtual_device;
	return directory;
}

Directory *
UpdateWalk::LockMakeVirtualDirectoryIfModified(Directory &parent,
					       std::string_view name,
					       const StorageFileInfo &info,
					       unsigned virtual_device) noexcept
{
	const ScopeDatabaseLock protect;
	return MakeVirtualDirectoryIfModified(parent, name,
					      info, virtual_device);
}
