// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
				std::string_view name, std::string_view suffix,
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

			FmtNotice(update_domain, "added {}/{}",
				  contdir->GetPath(),
				  song->filename);

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
