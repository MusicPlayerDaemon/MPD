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
#include "Service.hxx"
#include "UpdateIO.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseLock.hxx"
#include "db/Directory.hxx"
#include "db/Song.hxx"
#include "decoder/DecoderList.hxx"
#include "Log.hxx"

#include <unistd.h>

inline void
UpdateWalk::UpdateSongFile2(Directory &directory,
			    const char *name, const char *suffix,
			    const struct stat *st)
{
	db_lock();
	Song *song = directory.FindSong(name);
	db_unlock();

	if (!directory_child_access(directory, name, R_OK)) {
		FormatError(update_domain,
			    "no read permissions on %s/%s",
			    directory.GetPath(), name);
		if (song != nullptr) {
			db_lock();
			editor.DeleteSong(directory, song);
			db_unlock();
		}

		return;
	}

	if (!(song != nullptr && st->st_mtime == song->mtime &&
	      !walk_discard) &&
	    UpdateContainerFile(directory, name, suffix, st)) {
		if (song != nullptr) {
			db_lock();
			editor.DeleteSong(directory, song);
			db_unlock();
		}

		return;
	}

	if (song == nullptr) {
		FormatDebug(update_domain, "reading %s/%s",
			    directory.GetPath(), name);
		song = Song::LoadFile(name, directory);
		if (song == nullptr) {
			FormatDebug(update_domain,
				    "ignoring unrecognized file %s/%s",
				    directory.GetPath(), name);
			return;
		}

		db_lock();
		directory.AddSong(song);
		db_unlock();

		modified = true;
		FormatDefault(update_domain, "added %s/%s",
			      directory.GetPath(), name);
	} else if (st->st_mtime != song->mtime || walk_discard) {
		FormatDefault(update_domain, "updating %s/%s",
			      directory.GetPath(), name);
		if (!song->UpdateFile()) {
			FormatDebug(update_domain,
				    "deleting unrecognized file %s/%s",
				    directory.GetPath(), name);
			db_lock();
			editor.DeleteSong(directory, song);
			db_unlock();
		}

		modified = true;
	}
}

bool
UpdateWalk::UpdateSongFile(Directory &directory,
			   const char *name, const char *suffix,
			   const struct stat *st)
{
	if (!decoder_plugins_supports_suffix(suffix))
		return false;

	UpdateSongFile2(directory, name, suffix, st);
	return true;
}
