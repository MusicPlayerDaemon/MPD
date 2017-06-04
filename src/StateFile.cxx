/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "output/State.hxx"
#include "queue/PlaylistState.hxx"
#include "fs/io/TextFile.hxx"
#include "fs/io/FileOutputStream.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "mixer/Volume.hxx"
#include "SongLoader.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <exception>

#include <string.h>

static constexpr Domain state_file_domain("state_file");

constexpr std::chrono::steady_clock::duration StateFile::DEFAULT_INTERVAL;

StateFile::StateFile(AllocatedPath &&_path,
		     std::chrono::steady_clock::duration _interval,
		     Partition &_partition, EventLoop &_loop)
	:TimeoutMonitor(_loop),
	 path(std::move(_path)), path_utf8(path.ToUTF8()),
	 interval(_interval),
	 partition(_partition)
{
}

void
StateFile::RememberVersions() noexcept
{
	prev_volume_version = sw_volume_state_get_hash();
	prev_output_version = audio_output_state_get_version();
	prev_playlist_version = playlist_state_get_hash(partition.playlist,
							partition.pc);
}

bool
StateFile::IsModified() const noexcept
{
	return prev_volume_version != sw_volume_state_get_hash() ||
		prev_output_version != audio_output_state_get_version() ||
		prev_playlist_version != playlist_state_get_hash(partition.playlist,
								 partition.pc);
}

inline void
StateFile::Write(BufferedOutputStream &os)
{
	save_sw_volume_state(os);
	audio_output_state_save(os, partition.outputs);
	playlist_state_save(os, partition.playlist, partition.pc);
}

inline void
StateFile::Write(OutputStream &os)
{
	BufferedOutputStream bos(os);
	Write(bos);
	bos.Flush();
}

void
StateFile::Write()
{
	FormatDebug(state_file_domain,
		    "Saving state file %s", path_utf8.c_str());

	try {
		FileOutputStream fos(path);
		Write(fos);
		fos.Commit();
	} catch (const std::exception &e) {
		LogError(e);
	}

	RememberVersions();
}

void
StateFile::Read()
try {
	bool success;

	FormatDebug(state_file_domain, "Loading state file %s", path_utf8.c_str());

	TextFile file(path);

#ifdef ENABLE_DATABASE
	const SongLoader song_loader(partition.instance.database,
				     partition.instance.storage);
#else
	const SongLoader song_loader(nullptr, nullptr);
#endif

	const char *line;
	while ((line = file.ReadLine()) != nullptr) {
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
} catch (const std::exception &e) {
	LogError(e);
}

void
StateFile::CheckModified()
{
	if (!IsActive() && IsModified())
		Schedule(interval);
}

void
StateFile::OnTimeout()
{
	Write();
}
