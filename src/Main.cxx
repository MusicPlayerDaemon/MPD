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
#include "Main.hxx"
#include "Instance.hxx"
#include "CommandLine.hxx"
#include "PlaylistFile.hxx"
#include "MusicChunk.hxx"
#include "StateFile.hxx"
#include "player/Thread.hxx"
#include "Mapper.hxx"
#include "Permission.hxx"
#include "Listen.hxx"
#include "client/Listener.hxx"
#include "client/Client.hxx"
#include "client/ClientList.hxx"
#include "command/AllCommands.hxx"
#include "Partition.hxx"
#include "tag/Config.hxx"
#include "ReplayGainGlobal.hxx"
#include "Idle.hxx"
#include "Log.hxx"
#include "LogInit.hxx"
#include "input/Init.hxx"
#include "event/Loop.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Config.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "zeroconf/ZeroconfGlue.hxx"
#include "decoder/DecoderList.hxx"
#include "AudioParser.hxx"
#include "pcm/PcmConvert.hxx"
#include "unix/SignalHandlers.hxx"
#include "system/FatalError.hxx"
#include "thread/Slack.hxx"
#include "net/Init.hxx"
#include "lib/icu/Init.hxx"
#include "config/Global.hxx"
#include "config/Param.hxx"
#include "config/Defaults.hxx"
#include "config/Option.hxx"
#include "config/Domain.hxx"
#include "util/RuntimeError.hxx"

#ifdef ENABLE_DAEMON
#include "unix/Daemon.hxx"
#endif

#ifdef ENABLE_DATABASE
#include "db/update/Service.hxx"
#include "db/Configured.hxx"
#include "db/DatabasePlugin.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "storage/Configured.hxx"
#include "storage/CompositeStorage.hxx"
#ifdef ENABLE_INOTIFY
#include "db/update/InotifyUpdate.hxx"
#endif
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
#include "neighbor/Glue.hxx"
#endif

#ifdef ENABLE_SQLITE
#include "sticker/StickerDatabase.hxx"
#endif

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#ifdef ANDROID
#include "java/Global.hxx"
#include "java/File.hxx"
#include "android/Environment.hxx"
#include "android/Context.hxx"
#include "fs/StandardDirectory.hxx"
#include "fs/FileSystem.hxx"
#include "org_musicpd_Bridge.h"
#endif

#ifdef ENABLE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif

#ifdef ENABLE_DBUS
#include <dbus/dbus.h>
#endif

#include <stdlib.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <limits.h>

static constexpr size_t KILOBYTE = 1024;
static constexpr size_t MEGABYTE = 1024 * KILOBYTE;

static constexpr size_t DEFAULT_BUFFER_SIZE = 4 * MEGABYTE;

static constexpr
size_t MIN_BUFFER_SIZE = std::max(CHUNK_SIZE * 32,
				  64 * KILOBYTE);

static constexpr unsigned DEFAULT_BUFFER_BEFORE_PLAY = 10;

#ifdef ANDROID
Context *context;
#endif

Instance *instance;

struct Config {
	ReplayGainConfig replay_gain;
};

static Config
LoadConfig()
{
	return {LoadReplayGainConfig()};
}

#ifdef ENABLE_DAEMON

static void
glue_daemonize_init(const struct options *options)
{
	daemonize_init(config_get_string(ConfigOption::USER, nullptr),
		       config_get_string(ConfigOption::GROUP, nullptr),
		       config_get_path(ConfigOption::PID_FILE));

	if (options->kill)
		daemonize_kill();
}

#endif

static void
glue_mapper_init()
{
	mapper_init(config_get_path(ConfigOption::PLAYLIST_DIR));
}

#ifdef ENABLE_DATABASE

static void
InitStorage(EventLoop &event_loop)
{
	auto storage = CreateConfiguredStorage(event_loop);
	if (storage == nullptr)
		return;

	CompositeStorage *composite = new CompositeStorage();
	instance->storage = composite;
	composite->Mount("", std::move(storage));
}

/**
 * Returns the database.  If this function returns false, this has not
 * succeeded, and the caller should create the database after the
 * process has been daemonized.
 */
static bool
glue_db_init_and_load(void)
{
	instance->database =
		CreateConfiguredDatabase(instance->event_loop,
					 instance->io_thread.GetEventLoop(),
					 *instance);
	if (instance->database == nullptr)
		return true;

	if (instance->database->GetPlugin().flags & DatabasePlugin::FLAG_REQUIRE_STORAGE) {
		InitStorage(instance->io_thread.GetEventLoop());

		if (instance->storage == nullptr) {
			delete instance->database;
			instance->database = nullptr;
			LogDefault(config_domain,
				   "Found database setting without "
				   "music_directory - disabling database");
			return true;
		}
	} else {
		if (IsStorageConfigured())
			LogDefault(config_domain,
				   "Ignoring the storage configuration "
				   "because the database does not need it");
	}

	try {
		instance->database->Open();
	} catch (...) {
		std::throw_with_nested(std::runtime_error("Failed to open database plugin"));
	}

	if (!instance->database->IsPlugin(simple_db_plugin))
		return true;

	SimpleDatabase &db = *(SimpleDatabase *)instance->database;
	instance->update = new UpdateService(instance->event_loop, db,
					     static_cast<CompositeStorage &>(*instance->storage),
					     *instance);

	/* run database update after daemonization? */
	return db.FileExists();
}

static bool
InitDatabaseAndStorage()
{
	const bool create_db = !glue_db_init_and_load();
	return create_db;
}

#endif

/**
 * Configure and initialize the sticker subsystem.
 */
static void
glue_sticker_init()
{
#ifdef ENABLE_SQLITE
	auto sticker_file = config_get_path(ConfigOption::STICKER_FILE);
	if (sticker_file.IsNull())
		return;

	sticker_global_init(std::move(sticker_file));
#endif
}

static void
glue_state_file_init()
{
	auto path_fs = config_get_path(ConfigOption::STATE_FILE);
	if (path_fs.IsNull()) {
#ifdef ANDROID
		const auto cache_dir = GetUserCacheDir();
		if (cache_dir.IsNull())
			return;

		path_fs = cache_dir / Path::FromFS("state");
#else
		return;
#endif
	}

	const auto interval =
		config_get_unsigned(ConfigOption::STATE_FILE_INTERVAL,
				    StateFile::DEFAULT_INTERVAL);

	instance->state_file = new StateFile(std::move(path_fs), interval,
					     instance->partitions.front(),
					     instance->event_loop);
	instance->state_file->Read();
}

/**
 * Initialize the decoder and player core, including the music pipe.
 */
static void
initialize_decoder_and_player(const ReplayGainConfig &replay_gain_config)
{
	const ConfigParam *param;

	size_t buffer_size;
	param = config_get_param(ConfigOption::AUDIO_BUFFER_SIZE);
	if (param != nullptr) {
		char *test;
		long tmp = strtol(param->value.c_str(), &test, 10);
		if (*test != '\0' || tmp <= 0 || tmp == LONG_MAX)
			FormatFatalError("buffer size \"%s\" is not a "
					 "positive integer, line %i",
					 param->value.c_str(), param->line);
		buffer_size = tmp * KILOBYTE;

		if (buffer_size < MIN_BUFFER_SIZE) {
			FormatWarning(config_domain, "buffer size %lu is too small, using %lu bytes instead",
				      (unsigned long)buffer_size,
				      (unsigned long)MIN_BUFFER_SIZE);
			buffer_size = MIN_BUFFER_SIZE;
		}
	} else
		buffer_size = DEFAULT_BUFFER_SIZE;

	const unsigned buffered_chunks = buffer_size / CHUNK_SIZE;

	if (buffered_chunks >= 1 << 15)
		FormatFatalError("buffer size \"%lu\" is too big",
				 (unsigned long)buffer_size);

	float perc;
	param = config_get_param(ConfigOption::BUFFER_BEFORE_PLAY);
	if (param != nullptr) {
		char *test;
		perc = strtod(param->value.c_str(), &test);
		if (*test != '%' || perc < 0 || perc > 100) {
			FormatFatalError("buffered before play \"%s\" is not "
					 "a positive percentage and less "
					 "than 100 percent, line %i",
					 param->value.c_str(), param->line);
		}

		if (perc > 80) {
			/* this upper limit should avoid deadlocks
			   which can occur because the DecoderThread
			   cannot ever fill the music buffer to
			   exactly 100%; a few chunks always need to
			   be available to generate silence in
			   Player::SendSilence() */
			FormatError(config_domain,
				    "buffer_before_play is too large (%f%%), capping at 80%%; please fix your configuration",
				    perc);
			perc = 80;
		}
	} else
		perc = DEFAULT_BUFFER_BEFORE_PLAY;

	unsigned buffered_before_play = (perc / 100) * buffered_chunks;
	if (buffered_before_play > buffered_chunks)
		buffered_before_play = buffered_chunks;

	const unsigned max_length =
		config_get_positive(ConfigOption::MAX_PLAYLIST_LENGTH,
				    DEFAULT_PLAYLIST_MAX_LENGTH);

	AudioFormat configured_audio_format = AudioFormat::Undefined();
	param = config_get_param(ConfigOption::AUDIO_OUTPUT_FORMAT);
	if (param != nullptr) {
		try {
			configured_audio_format = ParseAudioFormat(param->value.c_str(),
								   true);
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("error parsing line %i",
								  param->line));
		}
	}

	instance->partitions.emplace_back(*instance,
					  "default",
					  max_length,
					  buffered_chunks,
					  buffered_before_play,
					  configured_audio_format,
					  replay_gain_config);
	auto &partition = instance->partitions.back();

	try {
		param = config_get_param(ConfigOption::REPLAYGAIN);
		if (param != nullptr)
			partition.replay_gain_mode =
				FromString(param->value.c_str());
	} catch (...) {
		std::throw_with_nested(FormatRuntimeError("Failed to parse line %i",
							  param->line));
	}
}

inline void
Instance::BeginShutdownUpdate() noexcept
{
#ifdef ENABLE_DATABASE
#ifdef ENABLE_INOTIFY
	mpd_inotify_finish();
#endif

	if (update != nullptr)
		update->CancelAllAsync();
#endif
}

inline void
Instance::FinishShutdownUpdate() noexcept
{
#ifdef ENABLE_DATABASE
	delete update;
#endif
}

inline void
Instance::ShutdownDatabase() noexcept
{
#ifdef ENABLE_DATABASE
	if (instance->database != nullptr) {
		instance->database->Close();
		delete instance->database;
	}

	delete instance->storage;
#endif
}

inline void
Instance::BeginShutdownPartitions() noexcept
{
	for (auto &partition : partitions) {
		partition.pc.Kill();
		partition.listener.reset();
	}
}

inline void
Instance::FinishShutdownPartitions() noexcept
{
	partitions.clear();
}

void
Instance::OnIdle(unsigned flags)
{
	/* send "idle" notifications to all subscribed
	   clients */
	client_list->IdleAdd(flags);

	if (flags & (IDLE_PLAYLIST|IDLE_PLAYER|IDLE_MIXER|IDLE_OUTPUT) &&
	    state_file != nullptr)
		state_file->CheckModified();
}

#ifndef ANDROID

int main(int argc, char *argv[])
{
#ifdef _WIN32
	return win32_main(argc, argv);
#else
	return mpd_main(argc, argv);
#endif
}

#endif

static int
mpd_main_after_fork(const Config &config);

#ifdef ANDROID
static inline
#endif
int mpd_main(int argc, char *argv[])
try {
	struct options options;

#ifdef ENABLE_DAEMON
	daemonize_close_stdin();
#endif

#ifndef ANDROID
#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
	setlocale(LC_COLLATE, "");
#endif
#endif

	IcuInit();

	const ScopeNetInit net_init;

	config_global_init();

#ifdef ANDROID
	(void)argc;
	(void)argv;

	const auto sdcard = Environment::getExternalStorageDirectory();
	if (!sdcard.IsNull()) {
		const auto config_path =
			sdcard / Path::FromFS("mpd.conf");
		if (FileExists(config_path))
			ReadConfigFile(config_path);
	}
#else
	ParseCommandLine(argc, argv, options);
#endif

	const auto config = LoadConfig();

#ifdef ENABLE_DAEMON
	glue_daemonize_init(&options);
#endif

	TagLoadConfig();

	log_init(options.verbose, options.log_stderr);

	instance = new Instance();

#ifdef ENABLE_NEIGHBOR_PLUGINS
	instance->neighbors = new NeighborGlue();
	instance->neighbors->Init(instance->io_thread.GetEventLoop(),
				  *instance);

	if (instance->neighbors->IsEmpty()) {
		delete instance->neighbors;
		instance->neighbors = nullptr;
	}
#endif

	const unsigned max_clients =
		config_get_positive(ConfigOption::MAX_CONN, 10);
	instance->client_list = new ClientList(max_clients);

	initialize_decoder_and_player(config.replay_gain);

	listen_global_init(*instance->partitions.front().listener);

#ifdef ENABLE_DAEMON
	daemonize_set_user();
	daemonize_begin(options.daemon);
#endif

	return mpd_main_after_fork(config);

} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}

static int
mpd_main_after_fork(const Config &config)
try {
	ConfigureFS();

	glue_mapper_init();

	initPermissions();
	spl_global_init();
#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif

	pcm_convert_global_init();

	decoder_plugin_init_all(GetGlobalConfig());

#ifdef ENABLE_DATABASE
	const bool create_db = InitDatabaseAndStorage();
#endif

	glue_sticker_init();

	command_init();

	for (auto &partition : instance->partitions) {
		partition.outputs.Configure(instance->rtio_thread.GetEventLoop(),
					    config.replay_gain,
					    partition.pc);
		partition.UpdateEffectiveReplayGainMode();
	}

	client_manager_init();
	input_stream_global_init(instance->io_thread.GetEventLoop());
	playlist_list_global_init();

#ifdef ENABLE_DAEMON
	daemonize_commit();
#endif

#ifndef ANDROID
	setup_log_output();

	SignalHandlersInit(instance->event_loop);
#endif

	instance->io_thread.Start();
	instance->rtio_thread.Start();

#ifdef ENABLE_NEIGHBOR_PLUGINS
	if (instance->neighbors != nullptr)
		instance->neighbors->Open();
#endif

	ZeroconfInit(instance->event_loop);

	for (auto &partition : instance->partitions)
		StartPlayerThread(partition.pc);

#ifdef ENABLE_DATABASE
	if (create_db) {
		/* the database failed to load: recreate the
		   database */
		unsigned job = instance->update->Enqueue("", true);
		if (job == 0)
			FatalError("directory update failed");
	}
#endif

	glue_state_file_init();

#ifdef ENABLE_DATABASE
	if (config_get_bool(ConfigOption::AUTO_UPDATE, false)) {
#ifdef ENABLE_INOTIFY
		if (instance->storage != nullptr &&
		    instance->update != nullptr)
			mpd_inotify_init(instance->event_loop,
					 *instance->storage,
					 *instance->update,
					 config_get_unsigned(ConfigOption::AUTO_UPDATE_DEPTH,
							     INT_MAX));
#else
		FormatWarning(config_domain,
			      "inotify: auto_update was disabled. enable during compilation phase");
#endif
	}
#endif

	config_global_check();

	/* enable all audio outputs (if not already done by
	   playlist_state_restore() */
	for (auto &partition : instance->partitions)
		partition.pc.LockUpdateAudio();

#ifdef _WIN32
	win32_app_started();
#endif

	/* the MPD frontend does not care about timer slack; set it to
	   a huge value to allow the kernel to reduce CPU wakeups */
	SetThreadTimerSlackMS(100);

#ifdef ENABLE_SYSTEMD_DAEMON
	sd_notify(0, "READY=1");
#endif

	/* run the main loop */
	instance->event_loop.Run();

#ifdef _WIN32
	win32_app_stopping();
#endif

	/* cleanup */

	instance->BeginShutdownUpdate();

	if (instance->state_file != nullptr) {
		instance->state_file->Write();
		delete instance->state_file;
	}

	ZeroconfDeinit();

	instance->BeginShutdownPartitions();

	delete instance->client_list;

#ifdef ENABLE_NEIGHBOR_PLUGINS
	if (instance->neighbors != nullptr) {
		instance->neighbors->Close();
		delete instance->neighbors;
	}
#endif

	instance->FinishShutdownUpdate();
	instance->ShutdownDatabase();

#ifdef ENABLE_SQLITE
	sticker_global_finish();
#endif

	playlist_list_global_finish();
	input_stream_global_finish();

#ifdef ENABLE_DATABASE
	mapper_finish();
#endif

	DeinitFS();

	instance->FinishShutdownPartitions();
	command_finish();
	decoder_plugin_deinit_all();
#ifdef ENABLE_ARCHIVE
	archive_plugin_deinit_all();
#endif
	config_global_finish();
	instance->rtio_thread.Stop();
	instance->io_thread.Stop();
#ifndef ANDROID
	SignalHandlersFinish();
#endif
	delete instance;
	instance = nullptr;

#ifdef ENABLE_DAEMON
	daemonize_finish();
#endif

	IcuFinish();

	log_deinit();

#ifdef ENABLE_DBUS
	/* free libdbus memory to make memory leak checkers happy */
	dbus_shutdown();
#endif

	return EXIT_SUCCESS;
} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}

#ifdef ANDROID

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_run(JNIEnv *env, jclass, jobject _context)
{
	Java::Init(env);
	Java::File::Initialise(env);
	Environment::Initialise(env);

	context = new Context(env, _context);

	mpd_main(0, nullptr);

	delete context;
	Environment::Deinitialise(env);
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_shutdown(JNIEnv *, jclass)
{
	if (instance != nullptr)
		instance->Break();
}

#endif
