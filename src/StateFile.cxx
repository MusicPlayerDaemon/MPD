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

#include "config.h"
#include "StateFile.hxx"
#include "output/OutputState.hxx"
#include "PlaylistState.hxx"
#include "fs/TextFile.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "mixer/Volume.hxx"
#include "SongLoader.hxx"
#include "fs/FileSystem.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <string.h>

static constexpr Domain state_file_domain("state_file");

StateFile::StateFile(AllocatedPath &&_path,
		     Partition &_partition, EventLoop &_loop)
	:TimeoutMonitor(_loop),
	 path(std::move(_path)), path_utf8(path.ToUTF8()),
	 partition(_partition),
	 prev_volume_version(0), prev_output_version(0),
	 prev_playlist_version(0)
{
}

void
StateFile::RememberVersions()
{
	prev_volume_version = sw_volume_state_get_hash();
	prev_output_version = audio_output_state_get_version();
	prev_playlist_version = playlist_state_get_hash(partition.playlist,
							partition.pc);
}

bool
StateFile::IsModified() const
{
	return prev_volume_version != sw_volume_state_get_hash() ||
		prev_output_version != audio_output_state_get_version() ||
		prev_playlist_version != playlist_state_get_hash(partition.playlist,
								 partition.pc);
}

void
StateFile::Write()
{
	FormatDebug(state_file_domain,
		    "Saving state file %s", path_utf8.c_str());

	FILE *fp = FOpen(path, FOpenMode::WriteText);
	if (gcc_unlikely(!fp)) {
		FormatErrno(state_file_domain, "failed to create %s",
			    path_utf8.c_str());
		return;
	}

	save_sw_volume_state(fp);
	audio_output_state_save(fp, partition.outputs);
	playlist_state_save(fp, partition.playlist, partition.pc);

	fclose(fp);

	RememberVersions();
}

void
StateFile::Read()
{
	bool success;

	FormatDebug(state_file_domain, "Loading state file %s", path_utf8.c_str());

	TextFile file(path);
	if (file.HasFailed()) {
		FormatErrno(state_file_domain, "failed to open %s",
			    path_utf8.c_str());
		return;
	}

#ifdef ENABLE_DATABASE
	const SongLoader song_loader(partition.instance.database);
#else
	const SongLoader song_loader(nullptr);
#endif

	const char *line;
	while ((line = file.ReadLine()) != NULL) {
		success = read_sw_volume_state(line, partition.outputs) ||
			audio_output_state_read(line, partition.outputs) ||
			playlist_state_restore(line, file, song_loader,
					       partition.playlist,
					       partition.pc);
		if (!success)
			FormatError(state_file_domain,
				    "Unrecognized line in state file: %s",
				    line);
	}

	RememberVersions();
}

void
StateFile::CheckModified()
{
	if (!IsActive() && IsModified())
		ScheduleSeconds(2 * 60);
}

void
StateFile::OnTimeout()
{
	Write();
}
