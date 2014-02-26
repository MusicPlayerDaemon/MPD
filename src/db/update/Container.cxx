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
#include "decoder/DecoderPlugin.hxx"
#include "decoder/DecoderList.hxx"
#include "fs/AllocatedPath.hxx"
#include "storage/FileInfo.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagBuilder.hxx"
#include "Log.hxx"

#include <sys/stat.h>

Directory *
UpdateWalk::MakeDirectoryIfModified(Directory &parent, const char *name,
				    const FileInfo &info)
{
	Directory *directory = parent.FindChild(name);

	// directory exists already
	if (directory != nullptr) {
		if (directory->IsMount())
			return nullptr;

		if (directory->mtime == info.mtime && !walk_discard) {
			/* not modified */
			return nullptr;
		}

		editor.DeleteDirectory(directory);
		modified = true;
	}

	directory = parent.MakeChild(name);
	directory->mtime = info.mtime;
	return directory;
}

static bool
SupportsContainerSuffix(const DecoderPlugin &plugin, const char *suffix)
{
	return plugin.container_scan != nullptr &&
		plugin.SupportsSuffix(suffix);
}

bool
UpdateWalk::UpdateContainerFile(Directory &directory,
				const char *name, const char *suffix,
				const FileInfo &info)
{
	const DecoderPlugin *_plugin = decoder_plugins_find([suffix](const DecoderPlugin &plugin){
			return SupportsContainerSuffix(plugin, suffix);
		});
	if (_plugin == nullptr)
		return false;
	const DecoderPlugin &plugin = *_plugin;

	db_lock();
	Directory *contdir = MakeDirectoryIfModified(directory, name, info);
	if (contdir == nullptr) {
		/* not modified */
		db_unlock();
		return true;
	}

	contdir->device = DEVICE_CONTAINER;
	db_unlock();

	const auto pathname = storage.MapFS(contdir->GetPath());
	if (pathname.IsNull()) {
		/* not a local file: skip, because the container API
		   supports only local files */
		editor.LockDeleteDirectory(contdir);
		return false;
	}

	char *vtrack;
	unsigned int tnum = 0;
	TagBuilder tag_builder;
	while ((vtrack = plugin.container_scan(pathname, ++tnum)) != nullptr) {
		Song *song = Song::NewFile(vtrack, *contdir);

		// shouldn't be necessary but it's there..
		song->mtime = info.mtime;

		const auto child_path_fs = AllocatedPath::Build(pathname,
								vtrack);
		plugin.ScanFile(child_path_fs,
				add_tag_handler, &tag_builder);

		tag_builder.Commit(song->tag);

		db_lock();
		contdir->AddSong(song);
		db_unlock();

		modified = true;

		FormatDefault(update_domain, "added %s/%s",
			      directory.GetPath(), vtrack);
		delete[] vtrack;
	}

	if (tnum == 1) {
		editor.LockDeleteDirectory(contdir);
		return false;
	} else
		return true;
}
