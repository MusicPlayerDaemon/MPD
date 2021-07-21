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
#include "UpdateIO.hxx"
#include "UpdateDomain.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "db/plugins/simple/Song.hxx"
#include "decoder/DecoderList.hxx"
#include "storage/FileInfo.hxx"
#include "Log.hxx"

#include <unistd.h>

inline void
UpdateWalk::UpdateSongFile2(Directory &directory,
			    const char *name, std::string_view suffix,
			    const StorageFileInfo &info) noexcept
try {
	Song *song;
	{
		const ScopeDatabaseLock protect;
		song = directory.FindSong(name);
	}

	if (!directory_child_access(storage, directory, name, R_OK)) {
		FmtError(update_domain,
			 "no read permissions on {}/{}",
			 directory.GetPath(), name);
		if (song != nullptr)
			editor.LockDeleteSong(directory, song);

		return;
	}

	if (!(song != nullptr && info.mtime == song->mtime && !walk_discard) &&
	    UpdateContainerFile(directory, name, suffix, info)) {
		if (song != nullptr)
			editor.LockDeleteSong(directory, song);

		return;
	}

	if (song == nullptr) {
		FmtDebug(update_domain, "reading {}/{}",
			 directory.GetPath(), name);

		auto new_song = Song::LoadFile(storage, name, directory);
		if (!new_song) {
			FmtDebug(update_domain,
				 "ignoring unrecognized file {}/{}",
				 directory.GetPath(), name);
			return;
		}

		{
			const ScopeDatabaseLock protect;
			directory.AddSong(std::move(new_song));
		}

		modified = true;
		FmtNotice(update_domain, "added {}/{}",
			  directory.GetPath(), name);
	} else if (info.mtime != song->mtime || walk_discard) {
		FmtNotice(update_domain, "updating {}/{}",
			  directory.GetPath(), name);
		if (!song->UpdateFile(storage)) {
			FmtDebug(update_domain,
				 "deleting unrecognized file {}/{}",
				 directory.GetPath(), name);
			editor.LockDeleteSong(directory, song);
		}

		modified = true;
	}
} catch (...) {
	FmtError(update_domain,
		 "error reading file {}/{}: {}",
		 directory.GetPath(), name, std::current_exception());
}

bool
UpdateWalk::UpdateSongFile(Directory &directory,
			   const char *name, std::string_view suffix,
			   const StorageFileInfo &info) noexcept
{
	if (!decoder_plugins_supports_suffix(suffix))
		return false;

	UpdateSongFile2(directory, name, suffix, info);
	return true;
}
