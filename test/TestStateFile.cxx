// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StateFile.hxx"
#include "Instance.hxx"
#include "Partition.hxx"
#include "output/Filtered.hxx"
#include "song/DetachedSong.hxx"
#include "config/Data.hxx"
#include "config/PartitionConfig.hxx"
#include "fs/FileSystem.hxx"
#include "io/FileOutputStream.hxx"
#include "io/FileLineReader.hxx"
#include "storage/CompositeStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "Log.hxx"
#include "LogBackend.hxx"
#include "util/StringCompare.hxx"
#include "util/StringStrip.hxx"
#include "event/FineTimerEvent.hxx"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define PARTITION_STATE          "partition: "
#define MOUNT_STATE_BEGIN        "mount_begin"
#define MOUNT_STATE_END          "mount_end"
#define MOUNT_STATE_STORAGE_URI  "uri: "
#define MOUNT_STATE_MOUNTED_URL  "mounted_url: "

Instance *global_instance = nullptr;

/**
 * Global test environment to initialize logging subsystem.
 * This allows FmtDebug/FmtError messages to be visible during test execution.
 */
class TestEnvironment final : public ::testing::Environment {
public:
	void SetUp() override {
		// Check if verbose logging is requested via environment variable
		const char *verbose = std::getenv("MPD_TEST_VERBOSE");
		if (verbose != nullptr &&
		    (std::string_view{verbose} == "1" ||
		     std::string_view{verbose} == "true")) {
			SetLogThreshold(verbose ? LogLevel::DEBUG : LogLevel::INFO);
		}

	}
};

/**
 * Test fixture for StateFile read/write operations.
 *
 * This fixture creates a minimal MPD instance with a temporary state file
 * for isolated testing of StateFile functionality.
 */
class TestStateFile : public ::testing::Test {
protected:
	TestStateFile()
		: temp_state_file(nullptr){}

	std::unique_ptr<Instance> instance;
	std::unique_ptr<StateFile> state_file;
	AllocatedPath temp_state_file;

	/**
	 * Set up test environment: create instance, partition, and state file.
	 * Called before each test case.
	 */
	void SetUp() override {
		// Create instance
		instance = std::make_unique<Instance>();
		global_instance = instance.get();

		// Generate unique temporary file path
		temp_state_file = GenerateTempFilePath();

		// Create configuration with audio output
		ConfigData config_data;

		// Add null audio output for testing
		ConfigBlock audio_output_block{1};
		audio_output_block.AddBlockParam("type", "null");
		audio_output_block.AddBlockParam("name", "MyTestOutput");
		audio_output_block.AddBlockParam("mixer_type", "null");
		config_data.AddBlock(ConfigBlockOption::AUDIO_OUTPUT,
		                     std::move(audio_output_block));

		// Create partition and add to instance
		PartitionConfig partition_config{config_data};
		instance->partitions.emplace_back(
			*instance,
			"default",
			partition_config
		);
		instance->partitions.emplace_back(
			*instance,
			"ExistingPartition",
			partition_config
		);

		// Get reference to the partition
		Partition &default_partition = instance->partitions.front();

		// Configure outputs from config data
		// Note: ReplayGainConfig needs to be created
		ReplayGainConfig replay_gain_config;
		replay_gain_config.preamp = 1.0;
		replay_gain_config.missing_preamp = 1.0;
		replay_gain_config.limit = true;

		default_partition.outputs.Configure(
			instance->event_loop,
			instance->rtio_thread.GetEventLoop(),
			config_data,
			replay_gain_config
		);

		// Set up composite storage for mount testing
		instance->storage = new CompositeStorage();

		// Create StateFile configuration with our temporary file
		StateFileConfig state_config{config_data};
		state_config.path = temp_state_file;

		// Create the StateFile for testing
		state_file = std::make_unique<StateFile>(
			std::move(state_config),
			instance->partitions.front(),
			instance->event_loop
		);
	}

	/**
	 * Clean up test environment: destroy state file and remove temp files.
	 * Called after each test case.
	 */
	void TearDown() override {
		// Destroy state file first
		state_file.reset();

		// Clean up storage if it was allocated
		if (instance->storage != nullptr) {
			delete instance->storage;
			instance->storage = nullptr;
		}

		// Clear global instance
		global_instance = nullptr;
		instance.reset();

		// Remove temporary file if it exists
		if (!temp_state_file.IsNull() && PathExists(temp_state_file)) {
			try {
				RemoveFile(temp_state_file);
			} catch (...) {
				// Ignore cleanup errors
			}
		}
	}

	/**
	 * Generate a unique temporary file path for state file testing.
	 * Uses timestamp and process ID to ensure uniqueness.
	 *
	 * @return AllocatedPath pointing to unique temporary file location
	 */
	[[nodiscard]]
	static AllocatedPath GenerateTempFilePath() noexcept {
		// Get current timestamp as nanoseconds since epoch
		const auto now = std::chrono::system_clock::now();
		const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
			now.time_since_epoch()
		).count();
		const std::string temp_dir = testing::TempDir();

		const auto base_path = AllocatedPath::FromFS(temp_dir);

		const auto filename = fmt::format("state_{}_{}",
		                                  timestamp, getpid());

		return AllocatedPath::Build(base_path,
		                            AllocatedPath::FromFS(filename.c_str()));
	}

	/**
	 * Write test content to the temporary state file.
	 *
	 * @param content The content to write (must be valid UTF-8)
	 * @throws std::exception on I/O error
	 */
	void WriteStateFile(std::string_view content) {
		FileOutputStream file{temp_state_file};

		// Convert string_view to span<const byte> for Write()
		const auto bytes = std::as_bytes(std::span{content});
		file.Write(bytes);

		file.Commit();
	}

	/**
	 * Get a reference to a partition by name.
	 *
	 * @param name Partition name to find (default: "default")
	 * @return Reference to the partition
	 * @throws Assertion failure if partition not found
	 */
	[[nodiscard]]
	Partition &GetPartition(std::string_view name = "default") noexcept {
		auto *partition = instance->FindPartition(name.data());
		assert(partition != nullptr);
		return *partition;
	}

	/**
	 * Get the value of a state file entry for a specific partition.
	 *
	 * This function reads the temporary state file and returns the value
	 * associated with the given key in the specified partition's section.
	 *
	 * @param partition_name Name of the partition ("default" for the first/unlabeled partition)
	 * @param key The state key to search for (e.g., "sw_volume", "state")
	 * @return The value if found, empty string if not found or on error
	 */
	[[nodiscard]]
	std::string GetStateFileEntry(std::string_view partition_name,
	                              std::string_view key) const {
		if (!FileExists(temp_state_file)) {
			return {};
		}

		try {
			// Open and read the state file
			FileLineReader reader{temp_state_file};

			// Track current partition (start with "default")
			std::string current_partition = "default";

			// Search key with colon separator
			const auto search_key = std::string{key} + ":";

			// Read file line by line
			const char *line;
			while ((line = reader.ReadLine()) != nullptr) {
				const char *value;

				// Check for partition switch
				if ((value = StringAfterPrefix(line, PARTITION_STATE)) != nullptr) {
					// Extract partition name after "partition: "
					current_partition = value;
					continue;
				}

				// Skip if we're not in the target partition
				if (current_partition != partition_name) {
					continue;
				}

				// Check if line matches our key
				if ((value = StringAfterPrefix(line, search_key)) != nullptr) {
					return std::string{StripLeft(value)};
				}
			}

			return {};
		} catch (...) {
			// File read error
			return {};
		}
	}

	/**
	 * Get all mounts from the state file.
	 *
	 * Mounts are global and not partition-specific. They appear in the
	 * default partition section of the state file.
	 *
	 * @return Vector of maps, where each map contains the key-value pairs for one mount
	 */
	[[nodiscard]]
	std::vector<std::map<std::string, std::string>>
	GetStateFileMounts() const {
		std::vector<std::map<std::string, std::string>> mounts;

		if (!FileExists(temp_state_file)) {
			return mounts;
		}

		try {
			FileLineReader reader{temp_state_file};
			std::map<std::string, std::string> current_mount;
			bool in_mount = false;
			const char *line;
			while ((line = reader.ReadLine()) != nullptr) {
				// Mounts should only in default partition section
				if (StringAfterPrefix(line, PARTITION_STATE) != nullptr) {
					break;
				}

				if (StringStartsWith(line, MOUNT_STATE_BEGIN)) {
					in_mount = true;
					current_mount.clear();
					continue;
				}

				if (!in_mount)
					continue;

				const char *value;
				if ((value = StringAfterPrefix(line, MOUNT_STATE_MOUNTED_URL)) != nullptr) {
					current_mount["mounted_url"] = StripLeft(value);
				} else if ((value = StringAfterPrefix(line, MOUNT_STATE_STORAGE_URI)) != nullptr) {
					current_mount["uri"] = StripLeft(value);
				} else if (StringStartsWith(line, MOUNT_STATE_END)) {
					if (!current_mount.empty()) {
						mounts.push_back(current_mount);
					}
					in_mount = false;
				}
			}

			return mounts;
		} catch (...) {
			return {};
		}
	}

	/**
	 * Print the contents of the state file for debugging.
	 */
	void DumpStateFile() const {
		if (!FileExists(temp_state_file)) {
			std::cerr << "State file does not exist\n";
			return;
		}

		std::cerr << "\n=== State File Contents ===\n";
		try {
			FileLineReader reader{temp_state_file};
			const char *line;
			while ((line = reader.ReadLine()) != nullptr) {
			std::cerr << line << "\n";
			}
		} catch (...) {
			std::cerr << "Error reading state file\n";
		}
		std::cerr << "=== End State File ===\n\n";
	}

	/**
	 * Helper to break the event loop.
	 */
	void BreakLoop() noexcept {
		instance->event_loop.Break();
	}
};

/**
 * Test that audio output configuration was properly loaded in SetUp().
 */
TEST_F(TestStateFile, AudioOutputLoadedFromConfig) {
	// Get the partition
	Partition &partition = GetPartition();

	// Verify exactly one audio output was configured
	ASSERT_EQ(partition.outputs.Size(), 1);

	// Get the output and verify its properties
	const auto &output = partition.outputs.Get(0);
	EXPECT_EQ(output.GetName(), "MyTestOutput");
	EXPECT_STREQ(output.GetPluginName(), "null");
}

/**
 * Test that partition configuration was properly loaded in SetUp().
 */
TEST_F(TestStateFile, PartitionLoadedFromConfig) {
	ASSERT_EQ(instance->partitions.size(), 2);
	ASSERT_NE(instance->FindPartition("ExistingPartition"), nullptr);
}

/**
 * Test that StateFile handles empty state file file gracefully.
 */
TEST_F(TestStateFile, ReadEmptyStateFile) {
	// Create an empty state file
	WriteStateFile("");

	// Should handle empty file without throwing
	state_file->Read();

	SUCCEED();
}

/**
 * Test that StateFile handles missing state file gracefully.
 * 
 * Reading a non-existent file should log an error.
 */
TEST_F(TestStateFile, ReadNonExistentFile) {
	testing::internal::CaptureStderr();

	state_file->Read();

	const std::string output = testing::internal::GetCapturedStderr();

	EXPECT_THAT(output, testing::HasSubstr("Failed to open"));
}

/**
 * Test that StateFile can successfully read a valid state file that contains the
 * default partition only.
 * 
 * There is no writing of the state file in this test.
 */
TEST_F(TestStateFile, ReadValidStateFile) {
	// Create a state file with valid content
	WriteStateFile(
		"sw_volume: 80\n"
		"state: stop\n"
		"random: 1\n"
		"repeat: 0\n"
	);

	state_file->Read();

	// Verify internal state
	EXPECT_NE(instance->FindPartition("default"), nullptr)
		<< "Default partition should have been created";
	EXPECT_EQ(GetPartition().mixer_memento.GetVolume(GetPartition().outputs), 80);
	EXPECT_EQ(GetPartition().pc.GetState(), PlayerState::STOP);
	EXPECT_TRUE(GetPartition().playlist.GetRandom());
	EXPECT_FALSE(GetPartition().playlist.GetRepeat());
}

/**
 * Test that StateFile correctly handles partition switching.
 * 
 * The state file format supports multiple partitions using "partition:" lines.
 */
TEST_F(TestStateFile, MultiplePartitions) {
	// Create a state file with multiple partitions
	WriteStateFile(
		"partition: secondary\n"
	);

	// Read the state file
	state_file->Read();

	// Verify that multiple partitions exist
	ASSERT_GE(instance->partitions.size(), 1);

	// The "secondary" partition should have been created (internal state)
	EXPECT_NE(instance->FindPartition("secondary"), nullptr)
		<< "Secondary partition should have been created";

	state_file->Write();

	// Check for anything in the secondary partition (state file on disk)
	EXPECT_EQ(GetStateFileEntry("secondary", "state"), "stop");

	// Check default partition also written
	EXPECT_EQ(GetStateFileEntry("default", "state"), "stop");
}

/**
 * Test reading and writing volume in two partitions.
 */
TEST_F(TestStateFile, VolumeMultiplePartitions) {
	// Create initial state file
	WriteStateFile(
		"sw_volume: 75\n"
		"partition: secondary\n"
		"sw_volume: 40\n"
	);

	state_file->Read();
	state_file->Write();

	// Validate specific entries were written
	EXPECT_EQ(GetStateFileEntry("default", "sw_volume"), "75");
	EXPECT_EQ(GetStateFileEntry("secondary", "sw_volume"), "40");
}

/**
 * Test reading and writing enabled audio output of a second partition.
 */
TEST_F(TestStateFile, AudioOutputSecondPartitionEnabled) {
	// Create initial state file
	WriteStateFile(
		"audio_device_state:0:MyTestOutput\n"
		"partition: secondary\n"
		"audio_device_state:1:MyTestOutput\n"
	);

	state_file->Read();
	state_file->Write();

	EXPECT_EQ(GetStateFileEntry("default", "audio_device_state:0"), "MyTestOutput");
	EXPECT_EQ(GetStateFileEntry("secondary", "audio_device_state:1"), "MyTestOutput");
}

/**
 * Test reading and writing disabled audio output of a second partition.
 */
TEST_F(TestStateFile, AudioOutputSecondPartitionDisabled) {
	// Create initial state file
	WriteStateFile(
		"audio_device_state:0:MyTestOutput\n"
		"partition: secondary\n"
		"audio_device_state:0:MyTestOutput\n"
	);

	state_file->Read();
	state_file->Write();

	EXPECT_EQ(GetStateFileEntry("default", "audio_device_state:0"), "MyTestOutput");
	EXPECT_EQ(GetStateFileEntry("secondary", "audio_device_state:0"), "MyTestOutput");
}

/**
 * Test reading and writing audio output of an existing partition.
 */
TEST_F(TestStateFile, AudioOutputExistingPartition) {
	// Create initial state file
	WriteStateFile(
		"audio_device_state:0:MyTestOutput\n"
		"partition: ExistingPartition\n"
		"audio_device_state:1:MyTestOutput\n"
	);

	state_file->Read();
	state_file->Write();

	EXPECT_EQ(GetStateFileEntry("default", "audio_device_state:0"), "MyTestOutput");
	EXPECT_EQ(GetStateFileEntry("ExistingPartition", "audio_device_state:1"), "MyTestOutput");
}

/**
 * Move audio output to an existing partition.
 */
TEST_F(TestStateFile, AudioOutputMoveToExistingPartition) {
	// Create initial state file
	WriteStateFile(
		"audio_device_state:1:MyTestOutput\n"   // start in default partition
	);

	state_file->Read();

	// Simulate a user moving output to the existing partition
	Partition &default_partition = GetPartition("default");
	Partition &existing_partition = GetPartition("ExistingPartition");
	auto *ao = default_partition.outputs.FindByName("MyTestOutput");
	existing_partition.outputs.AddMoveFrom(std::move(*ao), 1);

	state_file->Write();

	EXPECT_EQ(GetStateFileEntry("default", "audio_device_state:0"), "MyTestOutput");
	EXPECT_EQ(GetStateFileEntry("ExistingPartition", "audio_device_state:1"), "MyTestOutput");
}

/**
 * Move audio output from existing partition to default partition.
 */
TEST_F(TestStateFile, AudioOutputMoveToDefaultPartition) {
	// Create initial state file
	WriteStateFile(
		"partition: ExistingPartition\n"
		"audio_device_state:1:MyTestOutput\n"   // start in default partition
	);

	state_file->Read();

	// Simulate a user moving output to the default partition
	Partition &default_partition = GetPartition("default");
	auto *existing_output = default_partition.outputs.FindByName("MyTestOutput");
	auto *output = instance->FindOutput("MyTestOutput", default_partition);
	const bool was_enabled = output->IsEnabled();
	existing_output->ReplaceDummy(output->Steal(), was_enabled);

	state_file->Write();

	EXPECT_EQ(GetStateFileEntry("default", "audio_device_state:1"), "MyTestOutput");
	EXPECT_EQ(GetStateFileEntry("ExistingPartition", "audio_device_state:0"), "MyTestOutput");
}

/**
 * Test reading and writing playlist state across multiple partitions.
 */
TEST_F(TestStateFile, PlaylistStateMultiplePartitions) {
	// Write a known state file
	WriteStateFile(
		"state: stop\n"
		"random: 1\n"
		"repeat: 0\n"
		"playlist_begin\n"
		"playlist_end\n"
		"partition: secondary\n"
		"state: stop\n"
		"random: 0\n"
		"repeat: 1\n"
		"playlist_begin\n"
		"playlist_end\n"
	);

	state_file->Read();
	state_file->Write();

	EXPECT_EQ(GetStateFileEntry("default", "state"), "stop");
	EXPECT_EQ(GetStateFileEntry("default", "random"), "1");
	EXPECT_EQ(GetStateFileEntry("default", "repeat"), "0");
	EXPECT_EQ(GetStateFileEntry("secondary", "state"), "stop");
	EXPECT_EQ(GetStateFileEntry("secondary", "random"), "0");
	EXPECT_EQ(GetStateFileEntry("secondary", "repeat"), "1");
}

/**
 * Test reading and writing playlist songs in state across multiple partitions.
 *
 * Use a mock of playlist_check_translate_song to allow songs to load in all cases.
 * playlist_check_translate_song functionality is tested in test/test_translate_song.cxx.
 */
TEST_F(TestStateFile, PlaylistSongStateMultiplePartitions) {
	// Write a known state file with playlist songs
	WriteStateFile(
		"state: stop\n"
		"playlist_begin\n"
		"0:song1.mp3\n"
		"1:dir1/song2.mp3\n"
		"playlist_end\n"
		"partition: secondary\n"
		"state: stop\n"
		"playlist_begin\n"
		"0:secondary_song.mp3\n"
		"playlist_end\n"
	);

	state_file->Read();

	// Verify songs were loaded into default partition's queue
	Partition &default_partition = GetPartition("default");
	ASSERT_EQ(default_partition.playlist.queue.GetLength(), 2);
	EXPECT_STREQ(default_partition.playlist.queue.Get(0).GetURI(), "song1.mp3");
	EXPECT_STREQ(default_partition.playlist.queue.Get(1).GetURI(), "dir1/song2.mp3");

	// Verify song was loaded into secondary partition's queue
	const auto *secondary = instance->FindPartition("secondary");
	ASSERT_NE(secondary, nullptr);
	ASSERT_EQ(secondary->playlist.queue.GetLength(), 1);
	EXPECT_STREQ(secondary->playlist.queue.Get(0).GetURI(), "secondary_song.mp3");

	state_file->Write();

	EXPECT_EQ(GetStateFileEntry("default", "0"), "song1.mp3");
	EXPECT_EQ(GetStateFileEntry("default", "1"), "dir1/song2.mp3");

	EXPECT_EQ(GetStateFileEntry("secondary", "0"), "secondary_song.mp3");
}

/**
 * Test reading and writing storage mount state.
 */
TEST_F(TestStateFile, MountState) {
	// Create initial state file with a mount
	WriteStateFile(
		"mount_begin\n"
		"uri: music\n"
		"mounted_url: mock://server1/music\n"
		"mount_end\n"
	);

	state_file->Read();

	// Verify the mount was created
	auto *storage = instance->storage;
	ASSERT_NE(storage, nullptr);
	auto *composite = dynamic_cast<CompositeStorage*>(storage);
	ASSERT_NE(composite, nullptr);

	auto *mounted = composite->GetMount("music");
	EXPECT_NE(mounted, nullptr);

	state_file->Write();

	// Verify mount was written back
	auto mounts = GetStateFileMounts();

	// Check number of mounts
	EXPECT_EQ(mounts.size(), 1);

	// Check mount details
	EXPECT_EQ(mounts[0].at("uri"), "music");
	EXPECT_EQ(mounts[0].at("mounted_url"), "mock://server1/music");
}

/**
 * Test reading and writing multiple storage mounts.
 */
TEST_F(TestStateFile,MultipleMounts) {
	WriteStateFile(
		"mount_begin\n"
		"uri: music\n"
		"mounted_url: mock://server1/music\n"
		"mount_end\n"
		"mount_begin\n"
		"uri: podcasts\n"
		"mounted_url: mock://server2/podcasts\n"
		"mount_end\n"
	);

	state_file->Read();

	auto *composite = dynamic_cast<CompositeStorage*>(instance->storage);
	ASSERT_NE(composite, nullptr);

	// Verify both mounts exist (internal state)
	EXPECT_NE(composite->GetMount("music"), nullptr);
	EXPECT_NE(composite->GetMount("podcasts"), nullptr);

	state_file->Write();

	auto mounts = GetStateFileMounts();

	EXPECT_EQ(mounts.size(), 2);

	EXPECT_EQ(mounts[0].at("uri"), "music");
	EXPECT_EQ(mounts[0].at("mounted_url"), "mock://server1/music");

	EXPECT_EQ(mounts[1].at("uri"), "podcasts");
	EXPECT_EQ(mounts[1].at("mounted_url"), "mock://server2/podcasts");
}

/**
 * Test that malformed mount state is handled gracefully.
 * 
 * Errors should be logged.
 */
TEST_F(TestStateFile, MalformedMountState) {
	WriteStateFile(
		"mount_begin\n"
		"uri: incomplete\n"
		// Missing mounted_url and mount_end
		"state: stop\n"
	);

	testing::internal::CaptureStderr();

	// Should log error but not crash
	state_file->Read();

	const std::string output = testing::internal::GetCapturedStderr();

	EXPECT_THAT(output, testing::HasSubstr("Unrecognized line in mountpoint state: state: stop"));
	EXPECT_THAT(output, testing::HasSubstr("Missing value in mountpoint state."));
}

/**
 * Test unmount storage.
 * 
 * When a mount is unmounted, it should be removed from the state file.
 */
TEST_F(TestStateFile, UnmountRemovesMount) {
	// Initial mount
	WriteStateFile(
		"mount_begin\n"
		"uri: temp\n"
		"mounted_url: mock://temp/storage\n"
		"mount_end\n"
	);

	state_file->Read();

	// Verify mount exists (internal state)
	auto *composite = dynamic_cast<CompositeStorage*>(instance->storage);
	ASSERT_NE(composite, nullptr);
	ASSERT_NE(composite->GetMount("temp"), nullptr);

	// Unmount
	bool unmounted = composite->Unmount("temp");
	EXPECT_TRUE(unmounted);
	EXPECT_EQ(composite->GetMount("temp"), nullptr);

	state_file->Write();

	auto mounts = GetStateFileMounts();

	EXPECT_EQ(mounts.size(), 0);
}

/**
 * Test storage state with nested mount paths.
 * 
 * URIs with slashes should be handled correctly.
 */
TEST_F(TestStateFile, NestedMountPaths) {
	WriteStateFile(
		"mount_begin\n"
		"uri: music/classical\n"
		"mounted_url: mock://server/classical\n"
		"mount_end\n"
	);

	state_file->Read();
	state_file->Write();

	auto mounts = GetStateFileMounts();

	EXPECT_EQ(mounts.size(), 1);

	EXPECT_EQ(mounts[0].at("uri"), "music/classical");
	EXPECT_EQ(mounts[0].at("mounted_url"), "mock://server/classical");
}

/**
 * Test that StateFile handles malformed content gracefully.
 * 
 * Errors should be logged for malformed lines.
 */
TEST_F(TestStateFile, ReadMalformedStateFile) {
	// Create a state file with various malformed lines
	WriteStateFile(
		"invalid line without colon\n"
		":::too:many:colons:::\n"
		"incomplete:"
	);

	testing::internal::CaptureStderr();

	// Should handle malformed file gracefully (logs errors internally)
	state_file->Read();

	const std::string output = testing::internal::GetCapturedStderr();

	EXPECT_THAT(output, testing::HasSubstr("Unrecognized line in state file: invalid line without colon"));
	EXPECT_THAT(output, testing::HasSubstr("Unrecognized line in state file: :::too:many:colons:::"));
	EXPECT_THAT(output, testing::HasSubstr("Unrecognized line in state file: incomplete:"));
}

/**
 * Test that empty lines and whitespace-only lines are handled gracefully.
 * 
 * Errors should be logged for whitespace-only lines.
 */
TEST_F(TestStateFile, ReadWithEmptyLines) {
	WriteStateFile(
		"\n"
		"sw_volume: 100\n"
		"\n"
		"   \n"
		"state: play\n"
	);

	testing::internal::CaptureStderr();

	// Should skip empty/whitespace lines without error
	state_file->Read();

	const std::string output = testing::internal::GetCapturedStderr();

	EXPECT_THAT(output, testing::HasSubstr("Unrecognized line in state file:"));
	EXPECT_THAT(output, testing::HasSubstr("Unrecognized line in state file:"));
	EXPECT_THAT(output, testing::HasSubstr("Unrecognized line in state file:"));
}

/**
 * Test that CheckModified triggers a write when state has changed.
 */
TEST_F(TestStateFile, CheckModified) {
	// Create new config data with short save interval for testing
	ConfigData config_data;
	StateFileConfig state_config{config_data};
	state_config.path = temp_state_file;
	state_config.interval = std::chrono::milliseconds(10);

	state_file = std::make_unique<StateFile>(std::move(state_config),
						 GetPartition(),
						 instance->event_loop);

	// Initial write
	state_file->Write();

	// Verify initial state on disk (default is 100)
	EXPECT_EQ(GetStateFileEntry("default", "sw_volume"), "100");

	// Modify volume in partition
	GetPartition().mixer_memento.SetVolume(GetPartition().outputs, 50);

	// Trigger check - should schedule timer
	state_file->CheckModified();

	// Setup a timer to break the loop after 50ms (giving enough time for 10ms timer to fire)
	FineTimerEvent break_timer(instance->event_loop, BIND_THIS_METHOD(BreakLoop));
	break_timer.Schedule(std::chrono::milliseconds(50));

	// Run loop
	instance->event_loop.Run();

	// Check if file contains the new volume
	EXPECT_EQ(GetStateFileEntry("default", "sw_volume"), "50");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new TestEnvironment);
    return RUN_ALL_TESTS();
}
