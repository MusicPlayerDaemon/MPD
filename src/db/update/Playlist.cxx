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
#include "db/DatabaseLock.hxx"
#include "db/PlaylistVector.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "song/DetachedSong.hxx"
#include "input/InputStream.hxx"
#include "playlist/PlaylistPlugin.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "playlist/PlaylistStream.hxx"
#include "playlist/SongEnumerator.hxx"
#include "storage/FileInfo.hxx"
#include "storage/StorageInterface.hxx"
#include "util/StringFormat.hxx"
#include "Log.hxx"

void
UpdateWalk::UpdatePlaylistFile(Directory &parent, const char *name,
			       const StorageFileInfo &info,
			       const PlaylistPlugin &plugin) noexcept
{
	assert(plugin.open_stream);

	Directory *directory =
		LockMakeVirtualDirectoryIfModified(parent, name, info,
						   DEVICE_PLAYLIST);
	if (directory == nullptr)
		/* not modified */
		return;

	const auto uri_utf8 = storage.MapUTF8(directory->GetPath());

	FormatDebug(update_domain, "scanning playlist '%s'", uri_utf8.c_str());

	try {
		Mutex mutex;
		auto e = plugin.open_stream(InputStream::OpenReady(uri_utf8.c_str(),
								   mutex));
		if (!e) {
			/* unsupported URI? roll back.. */
			editor.LockDeleteDirectory(directory);
			return;
		}

		unsigned track = 0;

		while (true) {
			auto song = e->NextSong();
			if (!song)
				break;

			auto db_song = std::make_unique<Song>(std::move(*song),
							      *directory);
			db_song->target = "../" + db_song->filename;
			db_song->filename = StringFormat<64>("track%04u",
							     ++track);

			{
				const ScopeDatabaseLock protect;
				directory->AddSong(std::move(db_song));
			}
		}
	} catch (...) {
		FormatError(std::current_exception(),
			    "Failed to scan playlist '%s'", uri_utf8.c_str());
		editor.LockDeleteDirectory(directory);
	}
}

bool
UpdateWalk::UpdatePlaylistFile(Directory &directory,
			       const char *name, const char *suffix,
			       const StorageFileInfo &info) noexcept
{
	const auto *const plugin = FindPlaylistPluginBySuffix(suffix);
	if (plugin == nullptr)
		return false;

	if (plugin->as_folder)
		UpdatePlaylistFile(directory, name, info, *plugin);

	PlaylistInfo pi(name, info.mtime);

	const ScopeDatabaseLock protect;
	if (directory.playlists.UpdateOrInsert(std::move(pi)))
		modified = true;

	return true;
}
