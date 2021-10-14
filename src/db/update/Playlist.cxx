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
#include "UpdateDomain.hxx"
#include "db/DatabaseLock.hxx"
#include "db/PlaylistVector.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "song/DetachedSong.hxx"
#include "input/InputStream.hxx"
#include "playlist/PlaylistPlugin.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "playlist/PlaylistStream.hxx"
#include "playlist/SongEnumerator.hxx"
#include "storage/FileInfo.hxx"
#include "storage/StorageInterface.hxx"
#include "fs/Traits.hxx"
#include "util/StringFormat.hxx"
#include "Log.hxx"

inline void
UpdateWalk::UpdatePlaylistFile(Directory &directory,
			       SongEnumerator &contents) noexcept
{
	unsigned track = 0;

	while (true) {
		auto song = contents.NextSong();
		if (!song)
			break;

		auto db_song = std::make_unique<Song>(std::move(*song),
						      directory);
		const bool is_absolute =
			PathTraitsUTF8::IsAbsoluteOrHasScheme(db_song->filename.c_str());
		db_song->target = is_absolute
			? db_song->filename
			/* prepend "../" to relative paths to go from
			   the virtual directory (DEVICE_PLAYLIST) to
			   the containing directory */
			: "../" + db_song->filename;
		db_song->filename = StringFormat<64>("track%04u",
						     ++track);

		{
			const ScopeDatabaseLock protect;
			directory.AddSong(std::move(db_song));
		}
	}
}

inline void
UpdateWalk::UpdatePlaylistFile(Directory &parent, std::string_view name,
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

	FmtDebug(update_domain, "scanning playlist '{}'", uri_utf8);

	try {
		Mutex mutex;
		auto e = plugin.open_stream(InputStream::OpenReady(uri_utf8.c_str(),
								   mutex));
		if (!e) {
			/* unsupported URI? roll back.. */
			editor.LockDeleteDirectory(directory);
			return;
		}

		UpdatePlaylistFile(*directory, *e);

		if (directory->IsEmpty())
			editor.LockDeleteDirectory(directory);
	} catch (...) {
		FmtError(update_domain,
			 "Failed to scan playlist '{}': {}",
			 uri_utf8, std::current_exception());
		editor.LockDeleteDirectory(directory);
	}
}

bool
UpdateWalk::UpdatePlaylistFile(Directory &directory,
			       std::string_view name, std::string_view suffix,
			       const StorageFileInfo &info) noexcept
{
	const auto *const plugin = FindPlaylistPluginBySuffix(suffix);
	if (plugin == nullptr)
		return false;

	if (GetPlaylistPluginAsFolder(*plugin))
		UpdatePlaylistFile(directory, name, info, *plugin);

	PlaylistInfo pi(name, info.mtime);

	const ScopeDatabaseLock protect;
	if (directory.playlists.UpdateOrInsert(std::move(pi)))
		modified = true;

	return true;
}

void
UpdateWalk::PurgeDanglingFromPlaylists(Directory &directory) noexcept
{
	/* recurse */
	for (Directory &child : directory.children)
		PurgeDanglingFromPlaylists(child);

	if (!directory.IsPlaylist())
		/* this check is only for virtual directories
		   representing a playlist file */
		return;

	directory.ForEachSongSafe([&](Song &song){
		if (!song.target.empty() &&
		    !PathTraitsUTF8::IsAbsoluteOrHasScheme(song.target.c_str())) {
			Song *target = directory.LookupTargetSong(song.target.c_str());
			if (target == nullptr) {
				/* the target does not exist: remove
				   the virtual song */
				editor.DeleteSong(directory, &song);
				modified = true;
			} else {
				/* the target exists: mark it (for
				   option "hide_playlist_targets") */
				target->in_playlist = true;
			}
		}
	});
}
