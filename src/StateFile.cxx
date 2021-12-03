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

#include "config.h"
#include "StateFile.hxx"
#include "output/State.hxx"
#include "queue/PlaylistState.hxx"
#include "fs/io/TextFile.hxx"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "storage/StorageState.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "mixer/Volume.hxx"
#include "SongLoader.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <exception>

static constexpr Domain state_file_domain("state_file");

StateFile::StateFile(StateFileConfig &&_config,
		     Partition &_partition, EventLoop &_loop)
	:config(std::move(_config)), path_utf8(config.path.ToUTF8()),
	 timer_event(_loop, BIND_THIS_METHOD(OnTimeout)),
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
#ifdef ENABLE_DATABASE
	prev_storage_version = storage_state_get_hash(partition.instance);
#endif
}

bool
StateFile::IsModified() const noexcept
{
	return prev_volume_version != sw_volume_state_get_hash() ||
		prev_output_version != audio_output_state_get_version() ||
		prev_playlist_version != playlist_state_get_hash(partition.playlist,
								 partition.pc)
#ifdef ENABLE_DATABASE
		|| prev_storage_version != storage_state_get_hash(partition.instance)
#endif
		;
}

inline void
StateFile::Write(BufferedOutputStream &os)
{
	save_sw_volume_state(os);
	audio_output_state_save(os, partition.outputs);

#ifdef ENABLE_DATABASE
	storage_state_save(os, partition.instance);
#endif

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
	FmtDebug(state_file_domain,
		 "Saving state file {}", path_utf8);

	try {
		FileOutputStream fos(config.path);
		Write(fos);
		fos.Commit();
	} catch (...) {
		LogError(std::current_exception());
	}

	RememberVersions();
}

void
StateFile::Read()
try {
	bool success;

	FmtDebug(state_file_domain, "Loading state file {}", path_utf8);

	TextFile file(config.path);

#ifdef ENABLE_DATABASE
	const SongLoader song_loader(partition.instance.GetDatabase(),
				     partition.instance.storage);
#else
	const SongLoader song_loader(nullptr, nullptr);
#endif

	const char *line;
	while ((line = file.ReadLine()) != nullptr) {
		success = read_sw_volume_state(line, partition.outputs) ||
			audio_output_state_read(line, partition.outputs) ||
			playlist_state_restore(config, line, file, song_loader,
					       partition.playlist,
					       partition.pc);
#ifdef ENABLE_DATABASE
		success = success || storage_state_restore(line, file, partition.instance);
#endif

		if (!success)
			FmtError(state_file_domain,
				 "Unrecognized line in state file: {}",
				 line);
	}

	RememberVersions();
} catch (...) {
	LogError(std::current_exception());
}

void
StateFile::CheckModified() noexcept
{
	if (!timer_event.IsPending() && IsModified())
		timer_event.Schedule(config.interval);
}

void
StateFile::OnTimeout() noexcept
{
	Write();
}
