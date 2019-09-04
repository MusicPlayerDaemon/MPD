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

bool
UpdateWalk::UpdateContainerFile(Directory &directory,
				const char *name, const char *suffix,
				const StorageFileInfo &info) noexcept
{
	const DecoderPlugin *_plugin = decoder_plugins_find([suffix](const DecoderPlugin &plugin){
			return plugin.SupportsContainerSuffix(suffix);
		});
	if (_plugin == nullptr)
		return false;
	const DecoderPlugin &plugin = *_plugin;

	Directory *contdir;
	{
		const ScopeDatabaseLock protect;
		contdir = MakeVirtualDirectoryIfModified(directory, name,
							 info,
							 DEVICE_CONTAINER);
		if (contdir == nullptr)
			/* not modified */
			return true;
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
			auto song = std::make_unique<Song>(std::move(vtrack),
							   *contdir);

			// shouldn't be necessary but it's there..
			song->mtime = info.mtime;

			FormatDefault(update_domain, "added %s/%s",
				      contdir->GetPath(),
				      song->filename.c_str());

			{
				const ScopeDatabaseLock protect;
				contdir->AddSong(std::move(song));
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
