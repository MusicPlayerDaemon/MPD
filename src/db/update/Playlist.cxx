// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Walk.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseLock.hxx"
#include "db/PlaylistVector.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "db/plugins/simple/Song.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "song/DetachedSong.hxx"
#include "input/InputStream.hxx"
#include "input/WaitReady.hxx"
#include "playlist/PlaylistPlugin.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "playlist/PlaylistStream.hxx"
#include "playlist/SongEnumerator.hxx"
#include "storage/FileInfo.hxx"
#include "storage/StorageInterface.hxx"
#include "fs/Traits.hxx"
#include "Log.hxx"

#include <fmt/core.h>

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
		db_song->filename = fmt::format("track{:04}", ++track);

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

	const char *const path = directory->GetPath();

	FmtDebug(update_domain, "scanning playlist {:?}", path);

	try {
		Mutex mutex;
		auto is = storage.OpenFile(path, mutex);
		LockWaitReady(*is);

		auto e = plugin.open_stream(std::move(is));
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
			 "Failed to scan playlist {:?}: {}",
			 path, std::current_exception());
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
