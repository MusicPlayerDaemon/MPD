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
#include "Main.hxx"
#include "Instance.hxx"
#include "CommandLine.hxx"
#include "PlaylistFile.hxx"
#include "PlaylistGlobal.hxx"
#include "MusicChunk.hxx"
#include "StateFile.hxx"
#include "PlayerThread.hxx"
#include "Mapper.hxx"
#include "Permission.hxx"
#include "Listen.hxx"
#include "client/Client.hxx"
#include "client/ClientList.hxx"
#include "command/AllCommands.hxx"
#include "Partition.hxx"
#include "tag/TagConfig.hxx"
#include "ReplayGainConfig.hxx"
#include "Idle.hxx"
#include "Log.hxx"
#include "LogInit.hxx"
#include "GlobalEvents.hxx"
#include "input/Init.hxx"
#include "event/Loop.hxx"
#include "IOThread.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Config.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "zeroconf/ZeroconfGlue.hxx"
#include "decoder/DecoderList.hxx"
#include "AudioConfig.hxx"
#include "pcm/PcmConvert.hxx"
#include "unix/SignalHandlers.hxx"
#include "unix/Daemon.hxx"
#include "system/FatalError.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "thread/Id.hxx"
#include "thread/Slack.hxx"
#include "lib/icu/Init.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigData.hxx"
#include "config/ConfigDefaults.hxx"
#include "config/ConfigOption.hxx"
#include "config/ConfigError.hxx"
#include "Stats.hxx"

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

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <stdlib.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef __BLOCKS__
#include <dispatch/dispatch.h>
#endif

#include <limits.h>

static constexpr unsigned DEFAULT_BUFFER_SIZE = 4096;
static constexpr unsigned DEFAULT_BUFFER_BEFORE_PLAY = 10;

static constexpr Domain main_domain("main");

#ifdef ANDROID
Context *context;
#endif

Instance *instance;

static StateFile *state_file;

#ifndef ANDROID

static bool
glue_daemonize_init(const struct options *options, Error &error)
{
	auto pid_file = config_get_path(CONF_PID_FILE, error);
	if (pid_file.IsNull() && error.IsDefined())
		return false;

	daemonize_init(config_get_string(CONF_USER, nullptr),
		       config_get_string(CONF_GROUP, nullptr),
		       std::move(pid_file));

	if (options->kill)
		daemonize_kill();

	return true;
}

#endif

static bool
glue_mapper_init(Error &error)
{
	auto playlist_dir = config_get_path(CONF_PLAYLIST_DIR, error);
	if (playlist_dir.IsNull() && error.IsDefined())
		return false;

	mapper_init(std::move(playlist_dir));
	return true;
}

#ifdef ENABLE_DATABASE

static bool
InitStorage(Error &error)
{
	Storage *storage = CreateConfiguredStorage(io_thread_get(), error);
	if (storage == nullptr)
		return !error.IsDefined();

	assert(!error.IsDefined());

	CompositeStorage *composite = new CompositeStorage();
	instance->storage = composite;
	composite->Mount("", storage);
	return true;
}

/**
 * Returns the database.  If this function returns false, this has not
 * succeeded, and the caller should create the database after the
 * process has been daemonized.
 */
static bool
glue_db_init_and_load(void)
{
	Error error;
	instance->database =
		CreateConfiguredDatabase(*instance->event_loop, *instance,
					 error);
	if (instance->database == nullptr) {
		if (error.IsDefined())
			FatalError(error);
		else
			return true;
	}

	if (instance->database->GetPlugin().flags & DatabasePlugin::FLAG_REQUIRE_STORAGE) {
		if (!InitStorage(error))
			FatalError(error);

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

	if (!instance->database->Open(error))
		FatalError(error);

	if (!instance->database->IsPlugin(simple_db_plugin))
		return true;

	SimpleDatabase &db = *(SimpleDatabase *)instance->database;
	instance->update = new UpdateService(*instance->event_loop, db,
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
glue_sticker_init(void)
{
#ifdef ENABLE_SQLITE
	Error error;
	auto sticker_file = config_get_path(CONF_STICKER_FILE, error);
	if (sticker_file.IsNull()) {
		if (error.IsDefined())
			FatalError(error);
		return;
	}

	if (!sticker_global_init(std::move(sticker_file), error))
		FatalError(error);
#endif
}

static bool
glue_state_file_init(Error &error)
{
	auto path_fs = config_get_path(CONF_STATE_FILE, error);
	if (path_fs.IsNull()) {
		if (error.IsDefined())
			return false;

#ifdef ANDROID
		const auto cache_dir = GetUserCacheDir();
		if (cache_dir.IsNull())
			return true;

		path_fs = AllocatedPath::Build(cache_dir, "state");
#else
		return true;
#endif
	}

	unsigned interval = config_get_unsigned(CONF_STATE_FILE_INTERVAL,
						StateFile::DEFAULT_INTERVAL);

	state_file = new StateFile(std::move(path_fs), interval,
				   *instance->partition,
				   *instance->event_loop);
	state_file->Read();
	return true;
}

/**
 * Windows-only initialization of the Winsock2 library.
 */
static void winsock_init(void)
{
#ifdef WIN32
	WSADATA sockinfo;

	int retval = WSAStartup(MAKEWORD(2, 2), &sockinfo);
	if(retval != 0)
		FormatFatalError("Attempt to open Winsock2 failed; error code %d",
				 retval);

	if (LOBYTE(sockinfo.wVersion) != 2)
		FatalError("We use Winsock2 but your version is either too new "
			   "or old; please install Winsock 2.x");
#endif
}

/**
 * Initialize the decoder and player core, including the music pipe.
 */
static void
initialize_decoder_and_player(void)
{
	const struct config_param *param;

	size_t buffer_size;
	param = config_get_param(CONF_AUDIO_BUFFER_SIZE);
	if (param != nullptr) {
		char *test;
		long tmp = strtol(param->value.c_str(), &test, 10);
		if (*test != '\0' || tmp <= 0 || tmp == LONG_MAX)
			FormatFatalError("buffer size \"%s\" is not a "
					 "positive integer, line %i",
					 param->value.c_str(), param->line);
		buffer_size = tmp;
	} else
		buffer_size = DEFAULT_BUFFER_SIZE;

	buffer_size *= 1024;

	const unsigned buffered_chunks = buffer_size / CHUNK_SIZE;

	if (buffered_chunks >= 1 << 15)
		FormatFatalError("buffer size \"%lu\" is too big",
				 (unsigned long)buffer_size);

	float perc;
	param = config_get_param(CONF_BUFFER_BEFORE_PLAY);
	if (param != nullptr) {
		char *test;
		perc = strtod(param->value.c_str(), &test);
		if (*test != '%' || perc < 0 || perc > 100) {
			FormatFatalError("buffered before play \"%s\" is not "
					 "a positive percentage and less "
					 "than 100 percent, line %i",
					 param->value.c_str(), param->line);
		}
	} else
		perc = DEFAULT_BUFFER_BEFORE_PLAY;

	unsigned buffered_before_play = (perc / 100) * buffered_chunks;
	if (buffered_before_play > buffered_chunks)
		buffered_before_play = buffered_chunks;

	const unsigned max_length =
		config_get_positive(CONF_MAX_PLAYLIST_LENGTH,
				    DEFAULT_PLAYLIST_MAX_LENGTH);

	instance->partition = new Partition(*instance,
					    max_length,
					    buffered_chunks,
					    buffered_before_play);
}

/**
 * Handler for GlobalEvents::IDLE.
 */
static void
idle_event_emitted(void)
{
	/* send "idle" notifications to all subscribed
	   clients */
	unsigned flags = idle_get();
	if (flags != 0)
		instance->client_list->IdleAdd(flags);

	if (flags & (IDLE_PLAYLIST|IDLE_PLAYER|IDLE_MIXER|IDLE_OUTPUT) &&
	    state_file != nullptr)
		state_file->CheckModified();
}

#ifdef WIN32

/**
 * Handler for GlobalEvents::SHUTDOWN.
 */
static void
shutdown_event_emitted(void)
{
	instance->event_loop->Break();
}

#endif

#ifndef ANDROID

int main(int argc, char *argv[])
{
#ifdef WIN32
	return win32_main(argc, argv);
#else
	return mpd_main(argc, argv);
#endif
}

#endif

static int mpd_main_after_fork(struct options);

#ifdef ANDROID
static inline
#endif
int mpd_main(int argc, char *argv[])
{
	struct options options;
	Error error;

#ifndef ANDROID
	daemonize_close_stdin();

#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

#ifdef HAVE_GLIB
	g_set_application_name("Music Player Daemon");

#if !GLIB_CHECK_VERSION(2,32,0)
	/* enable GLib's thread safety code */
	g_thread_init(nullptr);
#endif
#endif
#endif

	if (!IcuInit(error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	winsock_init();
	io_thread_init();
	config_global_init();

#ifdef ANDROID
	(void)argc;
	(void)argv;

	{
		const auto sdcard = Environment::getExternalStorageDirectory();
		if (!sdcard.IsNull()) {
			const auto config_path =
				AllocatedPath::Build(sdcard, "mpd.conf");
			if (FileExists(config_path) &&
			    !ReadConfigFile(config_path, error)) {
				LogError(error);
				return EXIT_FAILURE;
			}
		}
	}
#else
	if (!parse_cmdline(argc, argv, &options, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	if (!glue_daemonize_init(&options, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}
#endif

	stats_global_init();
	TagLoadConfig();

	if (!log_init(options.verbose, options.log_stderr, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	instance = new Instance();
	instance->event_loop = new EventLoop();

#ifdef ENABLE_NEIGHBOR_PLUGINS
	instance->neighbors = new NeighborGlue();
	if (!instance->neighbors->Init(io_thread_get(), *instance, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	if (instance->neighbors->IsEmpty()) {
		delete instance->neighbors;
		instance->neighbors = nullptr;
	}
#endif

	const unsigned max_clients = config_get_positive(CONF_MAX_CONN, 10);
	instance->client_list = new ClientList(max_clients);

	initialize_decoder_and_player();

	if (!listen_global_init(*instance->event_loop, *instance->partition,
				error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

#ifndef ANDROID
	daemonize_set_user();
	daemonize_begin(options.daemon);
#endif

#ifdef __BLOCKS__
	/* Runs the OS X native event loop in the main thread, and runs
	   the rest of mpd_main on a new thread. This lets CoreAudio receive
	   route change notifications (e.g. plugging or unplugging headphones).
	   All hardware output on OS X ultimately uses CoreAudio internally.
	   This must be run after forking; if dispatch is called before forking,
	   the child process will have a broken internal dispatch state. */
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		exit(mpd_main_after_fork(options));
	});
	dispatch_main();
	return EXIT_FAILURE; // unreachable, because dispatch_main never returns
#else
	return mpd_main_after_fork(options);
#endif
}

static int mpd_main_after_fork(struct options options)
{
	Error error;

	GlobalEvents::Initialize(*instance->event_loop);
	GlobalEvents::Register(GlobalEvents::IDLE, idle_event_emitted);
#ifdef WIN32
	GlobalEvents::Register(GlobalEvents::SHUTDOWN, shutdown_event_emitted);
#endif

	ConfigureFS();

	if (!glue_mapper_init(error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	initPermissions();
	playlist_global_init();
	spl_global_init();
#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif

	if (!pcm_convert_global_init(error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	decoder_plugin_init_all();

#ifdef ENABLE_DATABASE
	const bool create_db = InitDatabaseAndStorage();
#endif

	glue_sticker_init();

	command_init();
	initAudioConfig();
	instance->partition->outputs.Configure(*instance->event_loop,
					       instance->partition->pc);
	client_manager_init();
	replay_gain_global_init();

	if (!input_stream_global_init(error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	playlist_list_global_init();

#ifndef ANDROID
	daemonize_commit();

	setup_log_output(options.log_stderr);

	SignalHandlersInit(*instance->event_loop);
#endif

	io_thread_start();

#ifdef ENABLE_NEIGHBOR_PLUGINS
	if (instance->neighbors != nullptr &&
	    !instance->neighbors->Open(error))
		FatalError(error);
#endif

	ZeroconfInit(*instance->event_loop);

	StartPlayerThread(instance->partition->pc);

#ifdef ENABLE_DATABASE
	if (create_db) {
		/* the database failed to load: recreate the
		   database */
		unsigned job = instance->update->Enqueue("", true);
		if (job == 0)
			FatalError("directory update failed");
	}
#endif

	if (!glue_state_file_init(error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	instance->partition->outputs.SetReplayGainMode(replay_gain_get_real_mode(instance->partition->playlist.queue.random));

#ifdef ENABLE_DATABASE
	if (config_get_bool(CONF_AUTO_UPDATE, false)) {
#ifdef ENABLE_INOTIFY
		if (instance->storage != nullptr &&
		    instance->update != nullptr)
			mpd_inotify_init(*instance->event_loop,
					 *instance->storage,
					 *instance->update,
					 config_get_unsigned(CONF_AUTO_UPDATE_DEPTH,
							     INT_MAX));
#else
		FormatWarning(main_domain,
			      "inotify: auto_update was disabled. enable during compilation phase");
#endif
	}
#endif

	config_global_check();

	/* enable all audio outputs (if not already done by
	   playlist_state_restore() */
	instance->partition->pc.UpdateAudio();

#ifdef WIN32
	win32_app_started();
#endif

	/* the MPD frontend does not care about timer slack; set it to
	   a huge value to allow the kernel to reduce CPU wakeups */
	SetThreadTimerSlackMS(100);

	/* run the main loop */
	instance->event_loop->Run();

#ifdef WIN32
	win32_app_stopping();
#endif

	/* cleanup */

#if defined(ENABLE_DATABASE) && defined(ENABLE_INOTIFY)
	mpd_inotify_finish();

	if (instance->update != nullptr)
		instance->update->CancelAllAsync();
#endif

	if (state_file != nullptr) {
		state_file->Write();
		delete state_file;
	}

	instance->partition->pc.Kill();
	ZeroconfDeinit();
	listen_global_finish();
	delete instance->client_list;

#ifdef ENABLE_NEIGHBOR_PLUGINS
	if (instance->neighbors != nullptr) {
		instance->neighbors->Close();
		delete instance->neighbors;
	}
#endif

#ifdef ENABLE_DATABASE
	delete instance->update;

	if (instance->database != nullptr) {
		instance->database->Close();
		delete instance->database;
	}

	delete instance->storage;
#endif

#ifdef ENABLE_SQLITE
	sticker_global_finish();
#endif

	GlobalEvents::Deinitialize();

	playlist_list_global_finish();
	input_stream_global_finish();

#ifdef ENABLE_DATABASE
	mapper_finish();
#endif

	delete instance->partition;
	command_finish();
	decoder_plugin_deinit_all();
#ifdef ENABLE_ARCHIVE
	archive_plugin_deinit_all();
#endif
	config_global_finish();
	io_thread_deinit();
#ifndef ANDROID
	SignalHandlersFinish();
#endif
	delete instance->event_loop;
	delete instance;
	instance = nullptr;
#ifndef ANDROID
	daemonize_finish();
#endif
#ifdef WIN32
	WSACleanup();
#endif

	IcuFinish();

	log_deinit();
	return EXIT_SUCCESS;
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
		instance->event_loop->Break();
}

#endif
