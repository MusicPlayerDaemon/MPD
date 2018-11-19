/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "UpdateDomain.hxx"
#include "song/DetachedSong.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "db/plugins/simple/Song.hxx"
#include "storage/StorageInterface.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "decoder/DecoderList.hxx"
#include "fs/AllocatedPath.hxx"
#include "storage/FileInfo.hxx"
#include "Log.hxx"

Directory *
UpdateWalk::MakeDirectoryIfModified(Directory &parent, const char *name,
				    const StorageFileInfo &info) noexcept
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
SupportsContainerSuffix(const DecoderPlugin &plugin,
			const char *suffix) noexcept
{
	return plugin.container_scan != nullptr &&
		plugin.SupportsSuffix(suffix);
}

bool
UpdateWalk::UpdateContainerFile(Directory &directory,
				const char *name, const char *suffix,
				const StorageFileInfo &info) noexcept
{
	const DecoderPlugin *_plugin = decoder_plugins_find([suffix](const DecoderPlugin &plugin){
			return SupportsContainerSuffix(plugin, suffix);
		});
	if (_plugin == nullptr)
		return false;
	const DecoderPlugin &plugin = *_plugin;

	Directory *contdir;
	{
		const ScopeDatabaseLock protect;
		contdir = MakeDirectoryIfModified(directory, name, info);
		if (contdir == nullptr)
			/* not modified */
			return true;

		contdir->device = DEVICE_CONTAINER;
	}

	const auto pathname = storage.MapFS(contdir->GetPath());
	if (pathname.IsNull()) {
		/* not a local file: skip, because the container API
		   supports only local files */
		editor.LockDeleteDirectory(contdir);
		return false;
	}

	try {
		auto v = plugin.container_scan(pathname);
		if (v.empty()) {
			editor.LockDeleteDirectory(contdir);
			return false;
		}

		for (auto &vtrack : v) {
			Song *song = Song::NewFrom(std::move(vtrack),
						   *contdir);

			// shouldn't be necessary but it's there..
			song->mtime = info.mtime;

			FormatDefault(update_domain, "added %s/%s",
				      contdir->GetPath(), song->uri);

			{
				const ScopeDatabaseLock protect;
				contdir->AddSong(song);
			}

			modified = true;
		}
	} catch (...) {
		LogError(std::current_exception());
		editor.LockDeleteDirectory(contdir);
		return false;
	}

	return true;
}
