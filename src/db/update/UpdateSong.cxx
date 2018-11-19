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
#include "UpdateIO.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "db/plugins/simple/Song.hxx"
#include "decoder/DecoderList.hxx"
#include "storage/FileInfo.hxx"
#include "Log.hxx"

#include <unistd.h>

inline void
UpdateWalk::UpdateSongFile2(Directory &directory,
			    const char *name, const char *suffix,
			    const StorageFileInfo &info) noexcept
{
	Song *song;
	{
		const ScopeDatabaseLock protect;
		song = directory.FindSong(name);
	}

	if (!directory_child_access(storage, directory, name, R_OK)) {
		FormatError(update_domain,
			    "no read permissions on %s/%s",
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
		FormatDebug(update_domain, "reading %s/%s",
			    directory.GetPath(), name);
		song = Song::LoadFile(storage, name, directory);
		if (song == nullptr) {
			FormatDebug(update_domain,
				    "ignoring unrecognized file %s/%s",
				    directory.GetPath(), name);
			return;
		}

		{
			const ScopeDatabaseLock protect;
			directory.AddSong(song);
		}

		modified = true;
		FormatDefault(update_domain, "added %s/%s",
			      directory.GetPath(), name);
	} else if (info.mtime != song->mtime || walk_discard) {
		FormatDefault(update_domain, "updating %s/%s",
			      directory.GetPath(), name);
		if (!song->UpdateFile(storage)) {
			FormatDebug(update_domain,
				    "deleting unrecognized file %s/%s",
				    directory.GetPath(), name);
			editor.LockDeleteSong(directory, song);
		}

		modified = true;
	}
}

bool
UpdateWalk::UpdateSongFile(Directory &directory,
			   const char *name, const char *suffix,
			   const StorageFileInfo &info) noexcept
{
	if (!decoder_plugins_supports_suffix(suffix))
		return false;

	UpdateSongFile2(directory, name, suffix, info);
	return true;
}
