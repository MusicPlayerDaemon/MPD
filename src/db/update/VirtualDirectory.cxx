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
