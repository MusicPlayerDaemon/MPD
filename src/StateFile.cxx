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
#include "config/PartitionConfig.hxx"
#include "Instance.hxx"
#include "SongLoader.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "util/StringCompare.hxx"

#include <exception>

#define PARTITION_STATE "partition: "

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
	for (auto &current_partition : partition.instance.partitions) {
		const bool is_default_partition =
			&current_partition == &partition.instance.partitions.front();
		if (!is_default_partition)  // Write partition header except for default partition
			os.Fmt(PARTITION_STATE "{}\n", current_partition.name);
		current_partition.mixer_memento.SaveSoftwareVolumeState(os);
		audio_output_state_save(os, current_partition.outputs);
		playlist_state_save(os, current_partition.playlist, current_partition.pc);

#ifdef ENABLE_DATABASE
		if (is_default_partition) {
			// Only save storage state once and do it in the default partition
			storage_state_save(os, partition.instance);
		}
#endif
	}

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

	Partition *current_partition = &partition;

	const char *line;
	while ((line = file.ReadLine()) != nullptr) {
		success = current_partition->mixer_memento.LoadSoftwareVolumeState(line,
						partition.outputs) ||
			audio_output_state_read(line, partition.outputs, current_partition) ||
			playlist_state_restore(config, line, file, song_loader,
						current_partition->playlist,
						current_partition->pc) ||
			PartitionSwitch(line, current_partition);

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

/**
 * Attempts to switch the current partition based on a state file line.
 *
 * @param line The line from the state file to parse
 * @param current_partition Reference to pointer that will be updated to point
 *                          to the target partition
 * @return true if the line was a partition switch command, false otherwise
 */
bool StateFile::PartitionSwitch(const char *line,
				Partition *&current_partition) noexcept {
	// Check if this line contains a partition switch command
	line = StringAfterPrefix(line, PARTITION_STATE);
	if (line == nullptr)
		return false;

	// Try to find existing partition
	Partition *new_partition = partition.instance.FindPartition(line);
	if (new_partition != nullptr) {
		current_partition = new_partition;
		FmtDebug(state_file_domain, "Switched to existing partition '{}'",
			 current_partition->name);
		return true;
	}

	// Partition doesn't exist, create it
	partition.instance.partitions.emplace_back(partition.instance, line,
						   PartitionConfig{});
	current_partition = &partition.instance.partitions.back();
	current_partition->UpdateEffectiveReplayGainMode();

	FmtDebug(state_file_domain, "Created partition '{}' and switched to it",
		 current_partition->name);

	return true;
}
