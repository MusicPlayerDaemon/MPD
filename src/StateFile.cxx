// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "StateFile.hxx"
#include "output/State.hxx"
#include "queue/PlaylistState.hxx"
#include "io/FileLineReader.hxx"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "storage/StorageState.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
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
	prev_volume_version = partition.mixer_memento.GetSoftwareVolumeStateHash();
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
	return prev_volume_version != partition.mixer_memento.GetSoftwareVolumeStateHash() ||
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
	partition.mixer_memento.SaveSoftwareVolumeState(os);
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

	FileLineReader file{config.path};

#ifdef ENABLE_DATABASE
	const SongLoader song_loader(partition.instance.GetDatabase(),
				     partition.instance.storage);
#else
	const SongLoader song_loader(nullptr, nullptr);
#endif

	const char *line;
	while ((line = file.ReadLine()) != nullptr) {
		success = partition.mixer_memento.LoadSoftwareVolumeState(line, partition.outputs) ||
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
