// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/**
 * MPD Integration Test Framework
 *
 * A testing framework for Music Player Daemon (MPD) that provides
 * automated setup, execution, and teardown of MPD test instances.
 *
 * This framework creates isolated MPD instances in temporary directories,
 * manages their lifecycle, and provides an API for sending commands
 * and verifying state changes.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <chrono>
#include <thread>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <utility>
#include <gtest/gtest.h>

#include "fs/Path.hxx"
#include "encoder/EncoderList.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "encoder/EncoderInterface.hxx"
#include "encoder/ToOutputStream.hxx"
#include "pcm/AudioFormat.hxx"
#include "config/Block.hxx"
#include "io/FileOutputStream.hxx"

namespace fs = std::filesystem;

/**
 * Test fixture for managing an MPD instance lifecycle.
 *
 * Provides management of:
 * - Temporary test directories
 * - MPD configuration files
 * - MPD process lifecycle
 * - Socket connections to MPD
 * - State file manipulation
 *
 */
class MpdTestFixture {
private:
	// Paths for test environment
	fs::path test_dir_;
	fs::path conf_path_;
	fs::path state_path_;
	fs::path pid_path_;
	fs::path db_path_;
	fs::path socket_path_;
	fs::path music_path_;

	// MPD process and connection state
	pid_t mpd_pid_ = -1;
	int sock_ = -1;
	std::string mpd_executable_ = "mpd";
	bool keep_dir_ = false;

	// Constants for timing and buffer sizes
	static constexpr int kDefaultStartupDelayMs = 1500;
	static constexpr int kDefaultShutdownDelayMs = 10;
	static constexpr std::size_t kSocketBufferSize = 1024;
	static constexpr std::size_t kGreetingBufferSize = 256;

	/**
	 * Create a dummy Ogg Vorbis file.
	 */
	void CreateDummyOggFile(const fs::path& path) {
		const auto *plugin = encoder_plugin_get("vorbis");
		if (plugin == nullptr) {
			throw std::runtime_error("Vorbis encoder plugin not found");
		}

		ConfigBlock block;
		block.AddBlockParam("quality", "0.1", -1);

		std::unique_ptr<PreparedEncoder> p_encoder(encoder_init(*plugin, block));
		if (p_encoder == nullptr) {
			throw std::runtime_error("Failed to init vorbis encoder");
		}

		AudioFormat audio_format(44100, SampleFormat::S16, 1); // Mono
		std::unique_ptr<Encoder> encoder(p_encoder->Open(audio_format));
		if (encoder == nullptr) {
			throw std::runtime_error("Failed to open vorbis encoder");
		}

		FileOutputStream os(Path::FromFS(path.c_str()));
		EncoderToOutputStream(os, *encoder);

		// A small amount of silence
		static constexpr std::byte silence[256]{};
		encoder->Write(std::span{silence});

		encoder->End();
		EncoderToOutputStream(os, *encoder);

		os.Commit();
	}

	/**
	 * Clean up all resources.
	 * Called by destructor, guaranteed not to throw.
	 */
	void Cleanup() noexcept {
		try {
			if (sock_ >= 0) {
				close(sock_);
				sock_ = -1;
			}
			if (mpd_pid_ > 0) {
				KillMpd();
			}
			if (!keep_dir_ && fs::exists(test_dir_)) {
				fs::remove_all(test_dir_);
			}
		} catch (...) {
			// Suppress all exceptions in cleanup
			std::cerr << "Warning: Exception during cleanup" << std::endl;
		}
	}

public:
	/**
	 * Construct a new test fixture.
	 * Creates a temporary directory for the test instance.
	 *
	 * @throws std::runtime_error if temporary directory creation fails
	 */
	MpdTestFixture() {
		// Create temporary directory
		char tmpl[] = "/tmp/mpd_test_XXXXXX";
		char* tmpdir_name = mkdtemp(tmpl);
		if (tmpdir_name == nullptr) {
			throw std::runtime_error("Failed to create temporary directory");
		}
		test_dir_ = fs::path(tmpdir_name);

		// Initialize all paths
		conf_path_ = test_dir_ / "mpd.conf";
		state_path_ = test_dir_ / "state";
		pid_path_ = test_dir_ / "pid";
		db_path_ = test_dir_ / "db";
		socket_path_ = test_dir_ / "socket";
		music_path_ = test_dir_ / "music";

		std::cout << "Test directory: " << test_dir_ << std::endl;
	}

	/**
	 * Destructor ensures all resources are cleaned up.
	 * Exception-safe - never throws.
	 */
	~MpdTestFixture() noexcept {
		Cleanup();
	}

	// Disable copy and move to prevent resource management issues
	MpdTestFixture(const MpdTestFixture&) = delete;
	MpdTestFixture& operator=(const MpdTestFixture&) = delete;
	MpdTestFixture(MpdTestFixture&&) = delete;
	MpdTestFixture& operator=(MpdTestFixture&&) = delete;

	/**
	 * Prevent the automatic deletion of the test directory upon destruction.
	 */
	void KeepDirOnExit() {
		keep_dir_ = true;
	}

	/**
	 * Create dummy song files in the music directory.
	 *
	 * @param song_files A list of song file paths relative to music dir
	 */
	void CreateDummySongs(const std::vector<std::string>& song_files) {
		if (!fs::exists(music_path_)) {
			fs::create_directory(music_path_);
		}
		for (const auto& song_file : song_files) {
			const auto path = music_path_ / song_file;
			fs::create_directories(path.parent_path());
			CreateDummyOggFile(path);
		}
	}

	/**
	 * Write a predefined state file before starting MPD.
	 * Useful for testing state restoration and persistence.
	 *
	 * @param content The content to write to the state file
	 * @throws std::runtime_error if file write fails
	 */
	void WriteStateFile(const std::string& content) {
		std::ofstream state_file(state_path_);
		if (!state_file) {
			throw std::runtime_error("Failed to open state file for writing");
		}
		state_file << content;
		if (!state_file) {
			throw std::runtime_error("Failed to write to state file");
		}
	}

	/**
	 * Generate and write MPD configuration file.
	 * Creates a minimal configuration with a null audio output.
	 *
	 * @param extra_config Optional additional configuration lines to append
	 * @throws std::runtime_error if file write fails
	 */
	void WriteConfig(const std::vector<std::string>& extra_config = {}) {
		std::ofstream conf_file(conf_path_);
		if (!conf_file) {
			throw std::runtime_error("Failed to open config file for writing");
		}

		conf_file << "state_file \"" << state_path_.string() << "\"\n";
		conf_file << "pid_file \"" << pid_path_.string() << "\"\n";
		if (fs::exists(music_path_)) {
			conf_file << "db_file \"" << db_path_.string() << "\"\n";
			conf_file << "music_directory \"" << music_path_.string() << "\"\n";
		}
		conf_file << "bind_to_address \"" << socket_path_.string() << "\"\n";
		conf_file << "audio_output {\n";
		conf_file << "    type \"null\"\n";
		conf_file << "    name \"MyTestOutput\"\n";
		conf_file << "    mixer_type \"null\"\n";
		conf_file << "}\n";

		// Add any extra configuration
		for (const auto& line : extra_config) {
			conf_file << line << "\n";
		}

		if (!conf_file) {
			throw std::runtime_error("Failed to write config file");
		}
	}

	/**
	 * Start the MPD process.
	 * Forks a child process and executes MPD with the test configuration.
	 *
	 * @param startup_delay_ms Time to wait for MPD to start (milliseconds)
	 * @return true if MPD was started successfully, false otherwise
	 */
	[[nodiscard]] bool StartMpd(int startup_delay_ms = kDefaultStartupDelayMs) {
		mpd_pid_ = fork();

		if (mpd_pid_ == 0) {
			// Child process - execute MPD
			std::vector<char> mpd_exec_buf(mpd_executable_.begin(), mpd_executable_.end());
			mpd_exec_buf.push_back('\0');

			std::string conf_str = conf_path_.string();
			std::vector<char> conf_buf(conf_str.begin(), conf_str.end());
			conf_buf.push_back('\0');

			char* argv[] = { mpd_exec_buf.data(), conf_buf.data(), nullptr };
			execv(mpd_exec_buf.data(), argv);

			// If we get here, execv failed
			perror("execv failed");
			exit(1);
		}

		if (mpd_pid_ < 0) {
			return false;
		}

		// Poll for MPD to start
		// Use a polling interval of 10ms, retrying until startup_delay_ms is reached
		for (int i = 0; i < startup_delay_ms / 10; ++i) {
			if (Connect()) {
				return true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return false; // MPD did not start within the timeout
	}

	/**
	 * Connect to the MPD socket.
	 * Establishes a Unix domain socket connection and reads the greeting.
	 *
	 * @return true if connection succeeded, false otherwise
	 */
	[[nodiscard]] bool Connect() {
		sock_ = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sock_ < 0) {
			perror("socket failed");
			return false;
		}

		struct sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

		if (::connect(sock_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
			close(sock_);
			sock_ = -1;
			return false;
		}

		// Read and display greeting
		char buffer[kGreetingBufferSize];
		ssize_t bytes_read = recv(sock_, buffer, sizeof(buffer) - 1, 0);
		if (bytes_read > 0) {
			buffer[bytes_read] = '\0';
			std::cout << "MPD greeting: " << buffer;
		}

		return true;
	}

	/**
	 * Send a command to MPD and receive the response.
	 * Automatically appends newline if not present.
	 *
	 * @param command The MPD protocol command to send
	 * @return The response from MPD, or error message if not connected
	 */
	[[nodiscard]] std::string SendCommand(const std::string& command) {
		if (sock_ < 0) {
			return "ERROR: Not connected";
		}

		std::string cmd = command;
		if (cmd.empty() || cmd.back() != '\n') {
			cmd += "\n";
		}

		ssize_t sent = send(sock_, cmd.c_str(), cmd.length(), 0);
		if (sent < 0) {
			return "ERROR: Send failed";
		}

		char buffer[kSocketBufferSize];
		ssize_t bytes_read = recv(sock_, buffer, sizeof(buffer) - 1, 0);
		if (bytes_read > 0) {
			buffer[bytes_read] = '\0';
			return std::string(buffer);
		}

		return "";
	}

	/**
	 * Send multiple commands in sequence.
	 * Each command is sent and its response collected.
	 *
	 * @param commands Vector of commands to send
	 * @return Vector of responses in the same order as commands
	 */
	[[nodiscard]] std::vector<std::string> SendCommands(
		const std::vector<std::string>& commands) {
		std::vector<std::string> responses;
		responses.reserve(commands.size());

		for (const auto& cmd : commands) {
			responses.push_back(SendCommand(cmd));
		}

		return responses;
	}

	/**
	 * Stop MPD gracefully using the kill command.
	 * Closes socket, waits for process termination, and allows filesystem sync.
	 *
	 * @param shutdown_delay_ms Time to wait after shutdown for filesystem sync
	 */
	void StopMpd(int shutdown_delay_ms = kDefaultShutdownDelayMs) {
		if (sock_ >= 0) {
			// Intentionally ignore response - MPD is shutting down
			[[maybe_unused]] auto response = SendCommand("kill");
			close(sock_);
			sock_ = -1;
		}

		if (mpd_pid_ > 0) {
			waitpid(mpd_pid_, nullptr, 0);
			mpd_pid_ = -1;
		}

		// Give filesystem time to sync state file
		std::this_thread::sleep_for(std::chrono::milliseconds(shutdown_delay_ms));
	}

	/**
	 * Force kill the MPD process using SIGTERM.
	 * Used when graceful shutdown fails or in cleanup.
	 */
	void KillMpd() noexcept {
		try {
			if (mpd_pid_ > 0) {
				kill(mpd_pid_, SIGTERM);
				waitpid(mpd_pid_, nullptr, 0);
				mpd_pid_ = -1;
			}
			if (sock_ >= 0) {
				close(sock_);
				sock_ = -1;
			}
		} catch (...) {
			// Suppress exceptions in force kill
		}
	}

	/**
	 * Read the entire state file contents.
	 *
	 * @return The complete contents of the state file
	 * @throws std::runtime_error if file cannot be read
	 */
	[[nodiscard]] std::string ReadStateFile() const {
		std::ifstream state_file(state_path_);
		if (!state_file) {
			throw std::runtime_error("Failed to open state file for reading");
		}

		std::stringstream buffer;
		buffer << state_file.rdbuf();
		return buffer.str();
	}

	/**
	 * Check if the state file contains a specific line.
	 * Useful for verifying state persistence.
	 *
	 * @param line The exact line to search for
	 * @return true if the line is found, false otherwise
	 */
	[[nodiscard]] bool StateFileContains(const std::string& line, const std::string& partition_name) const {
		std::ifstream state_file(state_path_);
		if (!state_file) {
			return false;
		}

		std::string current_partition_name = "default";
		bool in_correct_partition = (partition_name == "default");

		std::string file_line;
		while (std::getline(state_file, file_line)) {
			if (file_line.rfind("partition: ", 0) == 0) {
				current_partition_name = file_line.substr(11);
				in_correct_partition = (current_partition_name == partition_name);
			} else if (in_correct_partition && file_line == line) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Get the test directory path.
	 * @return Reference to the test directory path
	 */
	[[nodiscard]] const fs::path& GetTestDir() const noexcept {
		return test_dir_;
	}

	/**
	 * Get the state file path.
	 * @return Reference to the state file path
	 */
	[[nodiscard]] const fs::path& GetStatePath() const noexcept {
		return state_path_;
	}

	/**
	 * Get the socket path.
	 * @return Reference to the socket path
	 */
	[[nodiscard]] const fs::path& GetSocketPath() const noexcept {
		return socket_path_;
	}
};

// ============================================================================
// Google Test Integration
// ============================================================================

/**
 * Google Test fixture for MPD tests.
 * Automatically creates a fresh MpdTestFixture for each test.
 *
 * Use TEST_F(MpdTest, YourTestName) to define tests.
 */
class MpdTest : public ::testing::Test {
protected:
	std::unique_ptr<MpdTestFixture> fixture;

	/**
	 * Set up test fixture before each test.
	 * Creates a new MpdTestFixture instance.
	 */
	void SetUp() override {
		fixture = std::make_unique<MpdTestFixture>();
	}

	/**
	 * Tear down test fixture after each test.
	 * Ensures proper cleanup through RAII.
	 */
	void TearDown() override {
		fixture.reset();
	}
};

// ============================================================================
// Test Fixture for Pre-Populated Database
// ============================================================================

/**
 * Test fixture for tests that require a pre-populated song database.
 *
 * This fixture creates a database with dummy songs once for the entire
 * test suite. Each individual test case then gets a fresh copy of this
 * environment, allowing for isolated tests without the overhead of
 * populating the database for each one.
 */
class MpdPopulatedDbTest : public MpdTest {
protected:
	static std::unique_ptr<MpdTestFixture> template_fixture_;

	/**
	 * Set up the shared environment for the test suite.
	 */
	static void SetUpTestSuite() {
		template_fixture_ = std::make_unique<MpdTestFixture>();
		template_fixture_->KeepDirOnExit();

		template_fixture_->CreateDummySongs({"song1.ogg", "another/song2.ogg"});
		template_fixture_->WriteConfig();
		ASSERT_TRUE(template_fixture_->StartMpd());

		std::string response = template_fixture_->SendCommand("update");
		ASSERT_NE(response.find("updating_db:"), std::string::npos);

		// Poll status until update is finished
		bool finished = false;
		for (int i = 0; i < 50; ++i) { // max wait 2.5s
			response = template_fixture_->SendCommand("status");
			if (response.find("updating_db:") == std::string::npos) {
				finished = true;
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		ASSERT_TRUE(finished) << "Database update timed out in SetUpTestSuite.";

		template_fixture_->StopMpd();
	}

	/**
	 * Clean up the shared environment.
	 */
	static void TearDownTestSuite() {
		if (template_fixture_) {
			// The fixture is set to keep the directory, so we clean it up manually.
			fs::remove_all(template_fixture_->GetTestDir());
			template_fixture_.reset();
		}
	}

	/**
	 * Set up a fresh environment for each test case by copying the template.
	 */
	void SetUp() override {
		// Create the per-test fixture provided by MpdTest::SetUp()
		MpdTest::SetUp();

		// Copy the template environment into our test-specific directory
		ASSERT_TRUE(template_fixture_) << "Template fixture was not created.";
		for (const auto& entry : fs::directory_iterator(template_fixture_->GetTestDir())) {
			if (fs::is_socket(entry.symlink_status())) {
				continue;
			}
			fs::copy(entry.path(), fixture->GetTestDir() / entry.path().filename(),
				 fs::copy_options::recursive | fs::copy_options::copy_symlinks);
		}
	}
};

std::unique_ptr<MpdTestFixture> MpdPopulatedDbTest::template_fixture_;


// ============================================================================
// Test Cases
// ============================================================================

///
/// Test Cases for "Audio Output" State Persistence
///

/**
 * Test that audio outputs are enabled by default and placed in default partition.  No initial
 * state file.
 */
TEST_F(MpdTest, NoStateOutputEnabledByDefault) {
	fixture->WriteConfig();
	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	std::string response = fixture->SendCommand("outputs");
	EXPECT_TRUE(response.find("OK") != std::string::npos);
	EXPECT_TRUE(response.find("outputenabled: 1") != std::string::npos);  // check internal state

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:1:MyTestOutput", "default"));
}

/**
 * Test disabling an audio output and verifying state persistence. No initial state file.
 */
TEST_F(MpdTest, NoStateDisableOutput) {
	fixture->WriteConfig();
	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	std::string response = fixture->SendCommand("disableoutput 0");
	EXPECT_TRUE(response.find("OK") != std::string::npos);

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:0:MyTestOutput", "default"));
}

/**
 * Test enabling an output that starts in disabled state.
 */
TEST_F(MpdTest, LegacyStateEnableOutputFromDisabled) {
	// Start with disabled output
	fixture->WriteStateFile("audio_device_state:0:MyTestOutput\n");
	fixture->WriteConfig();

	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	std::string response = fixture->SendCommand("enableoutput 0");
	EXPECT_TRUE(response.find("OK") != std::string::npos);

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:1:MyTestOutput", "default"));
}

/**
 * Test that disabled outputs stays disabled.
 */
TEST_F(MpdTest, StateDefaultPartition) {
	fixture->WriteStateFile("audio_device_state:0:MyTestOutput\n");
	fixture->WriteConfig();
	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	std::string response = fixture->SendCommand("outputs");
	EXPECT_TRUE(response.find("OK") != std::string::npos);
	EXPECT_TRUE(response.find("outputenabled: 0") != std::string::npos);  // check internal state

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:0:MyTestOutput", "default"));
}

/**
 * Test that saved state file reflects assignment of enabled output to another
 * partition.  Output should remain disabled in the default partition.
 *
 * This also tests creation of a partition while reading state file.
 */
TEST_F(MpdTest, StateEnabledNonDefaultPartition) {
	fixture->WriteStateFile(
		"audio_device_state:0:MyTestOutput\n"
		"partition: TestPartition\n"
		"audio_device_state:1:MyTestOutput\n"
	);
	fixture->WriteConfig();
	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:0:MyTestOutput", "default"));
	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:1:MyTestOutput", "TestPartition"));
}

/**
 * Test that restore of state file reflects assignment of disabled output to non-default partition
 */
TEST_F(MpdTest, StateDisabledNonDefaultPartition) {
	fixture->WriteStateFile(
		"audio_device_state:0:MyTestOutput\n"
		"partition: TestPartition\n"
		"audio_device_state:0:MyTestOutput\n"
	);
	fixture->WriteConfig();
	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:0:MyTestOutput", "default"));
	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:0:MyTestOutput", "TestPartition"));
}

/**
 * Test that move from default partition disables the output in the default partitions.
 * The output should be enabled in the target partition.
 */
TEST_F(MpdTest, StateMovePartitionDisabledOutput) {
	fixture->WriteStateFile("audio_device_state:1:MyTestOutput\n");
	std::vector<std::string> extra_config = {
		"partition {",
		"    name \"TargetPartition\"",
		"}"
	};
	fixture->WriteConfig(extra_config);
	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	std::vector<std::string> commands = {
		"partition TargetPartition",
		"moveoutput MyTestOutput",
	};

	auto responses = fixture->SendCommands(commands);

	ASSERT_EQ(responses.size(), commands.size());

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:0:MyTestOutput", "default"));
	EXPECT_TRUE(fixture->StateFileContains("audio_device_state:1:MyTestOutput",
						"TargetPartition"));
}

///
/// Test Cases for "Player Control" State Persistence
///

/**
 * Test enabling a mode on default partition.
 */
TEST_F(MpdTest, StateChangeConsumeModeOnDefaultPartition) {
	// Start with disabled output
	fixture->WriteStateFile("consume: 0\n");
	fixture->WriteConfig();

	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	std::string response = fixture->SendCommand("consume 1");
	EXPECT_TRUE(response.find("OK") != std::string::npos);

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("consume: 1", "default"));
}

/**
 * Test enabling a mode on non-default partition.
 */
TEST_F(MpdTest, StateChangeConsumeModeOnNonDefaultPartition) {
	// Start with disabled output
	fixture->WriteStateFile("partition: TestPartition\n"
				"consume: 0\n");
	fixture->WriteConfig();

	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	std::vector<std::string> commands = {
		"partition TestPartition",
		"consume 1",
	};

	auto responses = fixture->SendCommands(commands);

	ASSERT_EQ(responses.size(), commands.size());

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("consume: 1", "TestPartition"));
}

///
/// Test Cases for "Mixer Volume" State Persistence
///

/**
 * Test setting volume of an output on default partition.
 */
TEST_F(MpdTest, StateChangeVolumeOnDefaultPartition) {
	// Start with initial volume on default partition
	fixture->WriteStateFile("sw_volume: 11\n");
	fixture->WriteConfig();

	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	// Confirm initial internal state
	std::string response = fixture->SendCommand("getvol");
	EXPECT_TRUE(response.find("OK") != std::string::npos);
	EXPECT_TRUE(response.find("volume: 11") != std::string::npos);  // check internal state

	// Change the volume
	std::string response2 = fixture->SendCommand("setvol 12");
	EXPECT_TRUE(response2.find("OK") != std::string::npos);

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("sw_volume: 12", "default"));
}

/**
 * Test setting volume of an output on non-default partition.
 */
TEST_F(MpdTest, StateChangeVolumeModeOnNonDefaultPartition) {
	// Start with initial volume on non-default partition
	fixture->WriteStateFile("partition: TestPartition\n"
				"sw_volume:11\n"
				"audio_device_state:1:MyTestOutput\n");
	fixture->WriteConfig();

	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	// Change the volume
	std::vector<std::string> commands = {
		"partition TestPartition",
		"setvol 12",
	};

	auto responses = fixture->SendCommands(commands);

	ASSERT_EQ(responses.size(), commands.size());

	// Each response should not be empty
	for (const auto& resp : responses) {
		EXPECT_FALSE(resp.empty()) << "Response should not be empty";
	}

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("sw_volume: 12", "TestPartition"));
}

///
/// Test Cases for "Playlist" State Persistence
///

/**
 * Test setting of a playlist on default partition.
 *
 * This tests playlist state on exit of MPD (playlist created by user after MPD starts).
 */
TEST_F(MpdPopulatedDbTest, PlaylistOnDefaultPartition) {
	fixture->WriteConfig();
	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	// Add the songs to the playlist
	std::vector<std::string> commands = {
		"add song1.ogg",
		"add another/song2.ogg",
	};

	auto responses = fixture->SendCommands(commands);

	ASSERT_EQ(responses.size(), commands.size());

	// Each response should not be empty
	for (const auto& resp : responses) {
		EXPECT_FALSE(resp.empty()) << "Response should not be empty";
	}

	fixture->StopMpd();

	EXPECT_TRUE(fixture->StateFileContains("0:song1.ogg", "default"));
	EXPECT_TRUE(fixture->StateFileContains("1:another/song2.ogg", "default"));
}

/**
 * Test setting of a playlist on non-default partition.
 *
 * This tests playlist state on exit of MPD (playlist created by user after MPD starts).
 */
TEST_F(MpdPopulatedDbTest, PlaylistOnNonDefaultPartition) {
	fixture->WriteStateFile("partition: TestPartition\n");
	fixture->WriteConfig();
	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	// Add the songs to the playlist
	std::vector<std::string> commands = {
		"partition TestPartition",
		"add song1.ogg",
		"add another/song2.ogg",
	};

	auto responses = fixture->SendCommands(commands);

	ASSERT_EQ(responses.size(), commands.size());

	// Each response should not be empty
	for (const auto& resp : responses) {
		EXPECT_FALSE(resp.empty()) << "Response should not be empty";
	}

	fixture->StopMpd();

	EXPECT_EQ(fixture->StateFileContains("0:song1.ogg", "default"), false);
	EXPECT_EQ(fixture->StateFileContains("1:another/song2.ogg", "default"), false);
	EXPECT_TRUE(fixture->StateFileContains("0:song1.ogg", "TestPartition"));
	EXPECT_TRUE(fixture->StateFileContains("1:another/song2.ogg", "TestPartition"));
}

/**
 * Test loading playlist from state file on non-default partition.
 *
 * This tests both loading and saving of playlists from state file..
 */
TEST_F(MpdPopulatedDbTest, StatePlaylistOnNonDefaultPartition) {
	fixture->WriteStateFile(
		"state: stop\n"  // Required by parsing logic in src/queue/PlaylistState.cxx
		"playlist_begin\n"
		// Empty default partition playlist for catching possible error in parsing
		"playlist_end\n"
		"partition: TestPartition\n"
		"state: stop\n"  // Required by parsing logic in src/queue/PlaylistState.cxx
		"playlist_begin\n"
		"0:song1.ogg\n"
		"1:another/song2.ogg\n"
		"playlist_end\n"
	);
	fixture->WriteConfig();
	ASSERT_TRUE(fixture->StartMpd());
	ASSERT_TRUE(fixture->Connect());

	fixture->StopMpd();

	EXPECT_EQ(fixture->StateFileContains("0:song1.ogg", "default"), false);
	EXPECT_EQ(fixture->StateFileContains("1:another/song2.ogg", "default"), false);
	EXPECT_TRUE(fixture->StateFileContains("0:song1.ogg", "TestPartition"));
	EXPECT_TRUE(fixture->StateFileContains("1:another/song2.ogg", "TestPartition"));
}

/**
 * Main entry point - initializes and runs all Google Test cases.
 */
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}