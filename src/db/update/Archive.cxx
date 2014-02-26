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
#include "UpdateDomain.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "db/plugins/simple/Song.hxx"
#include "storage/StorageInterface.hxx"
#include "fs/AllocatedPath.hxx"
#include "storage/FileInfo.hxx"
#include "archive/ArchiveList.hxx"
#include "archive/ArchivePlugin.hxx"
#include "archive/ArchiveFile.hxx"
#include "archive/ArchiveVisitor.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <string>

#include <sys/stat.h>
#include <string.h>

void
UpdateWalk::UpdateArchiveTree(Directory &directory, const char *name)
{
	const char *tmp = strchr(name, '/');
	if (tmp) {
		const std::string child_name(name, tmp);
		//add dir is not there already
		db_lock();
		Directory *subdir =
			directory.MakeChild(child_name.c_str());
		subdir->device = DEVICE_INARCHIVE;
		db_unlock();

		//create directories first
		UpdateArchiveTree(*subdir, tmp + 1);
	} else {
		if (strlen(name) == 0) {
			LogWarning(update_domain,
				   "archive returned directory only");
			return;
		}

		//add file
		db_lock();
		Song *song = directory.FindSong(name);
		db_unlock();
		if (song == nullptr) {
			song = Song::LoadFile(storage, name, directory);
			if (song != nullptr) {
				db_lock();
				directory.AddSong(song);
				db_unlock();

				modified = true;
				FormatDefault(update_domain, "added %s/%s",
					      directory.GetPath(), name);
			}
		}
	}
}

class UpdateArchiveVisitor final : public ArchiveVisitor {
	UpdateWalk &walk;
	Directory *directory;

 public:
	UpdateArchiveVisitor(UpdateWalk &_walk, Directory *_directory)
		:walk(_walk), directory(_directory) {}

	virtual void VisitArchiveEntry(const char *path_utf8) override {
		FormatDebug(update_domain,
			    "adding archive file: %s", path_utf8);
		walk.UpdateArchiveTree(*directory, path_utf8);
	}
};

/**
 * Updates the file listing from an archive file.
 *
 * @param parent the parent directory the archive file resides in
 * @param name the UTF-8 encoded base name of the archive file
 * @param st stat() information on the archive file
 * @param plugin the archive plugin which fits this archive type
 */
void
UpdateWalk::UpdateArchiveFile(Directory &parent, const char *name,
			      const FileInfo &info,
			      const ArchivePlugin &plugin)
{
	db_lock();
	Directory *directory = parent.FindChild(name);
	db_unlock();

	if (directory != nullptr && directory->mtime == info.mtime &&
	    !walk_discard)
		/* MPD has already scanned the archive, and it hasn't
		   changed since - don't consider updating it */
		return;

	const auto path_fs = storage.MapChildFS(parent.GetPath(), name);
	if (path_fs.IsNull())
		/* not a local file: skip, because the archive API
		   supports only local files */
		return;

	/* open archive */
	Error error;
	ArchiveFile *file = archive_file_open(&plugin, path_fs, error);
	if (file == nullptr) {
		LogError(error);
		if (directory != nullptr)
			editor.LockDeleteDirectory(directory);
		return;
	}

	FormatDebug(update_domain, "archive %s opened", path_fs.c_str());

	if (directory == nullptr) {
		FormatDebug(update_domain,
			    "creating archive directory: %s", name);
		db_lock();
		directory = parent.CreateChild(name);
		/* mark this directory as archive (we use device for
		   this) */
		directory->device = DEVICE_INARCHIVE;
		db_unlock();
	}

	directory->mtime = info.mtime;

	UpdateArchiveVisitor visitor(*this, directory);
	file->Visit(visitor);
	file->Close();
}

bool
UpdateWalk::UpdateArchiveFile(Directory &directory,
			      const char *name, const char *suffix,
			      const FileInfo &info)
{
	const ArchivePlugin *plugin = archive_plugin_from_suffix(suffix);
	if (plugin == nullptr)
		return false;

	UpdateArchiveFile(directory, name, info, *plugin);
	return true;
}
