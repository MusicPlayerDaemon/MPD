// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
			    std::string_view name, std::string_view suffix,
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
		return;
	}

	if (!(song != nullptr && info.mtime == song->mtime && !walk_discard) &&
	    UpdateContainerFile(directory, name, suffix, info)) {
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

		new_song->mark = true;

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
		if (song->UpdateFile(storage))
			song->mark = true;
		else
			FmtDebug(update_domain,
				 "deleting unrecognized file {}/{}",
				 directory.GetPath(), name);

		modified = true;
	} else {
		/* not modified */
		song->mark = true;
	}
} catch (...) {
	FmtError(update_domain,
		 "error reading file {}/{}: {}",
		 directory.GetPath(), name, std::current_exception());
}

bool
UpdateWalk::UpdateSongFile(Directory &directory,
			   std::string_view name, std::string_view suffix,
			   const StorageFileInfo &info) noexcept
{
	if (!decoder_plugins_supports_suffix(suffix))
		return false;

	UpdateSongFile2(directory, name, suffix, info);
	return true;
}
