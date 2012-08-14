/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "main.h"
#include "daemon.h"
#include "io_thread.h"
#include "client.h"
#include "client_idle.h"
#include "idle.h"
#include "command.h"
#include "playlist.h"
#include "stored_playlist.h"
#include "database.h"
#include "update.h"
#include "player_thread.h"
#include "listen.h"
#include "cmdline.h"
#include "conf.h"
#include "path.h"
#include "mapper.h"
#include "chunk.h"
#include "player_control.h"
#include "stats.h"
#include "sig_handlers.h"
#include "audio_config.h"
#include "output_all.h"
#include "volume.h"
#include "log.h"
#include "permission.h"
#include "pcm_resample.h"
#include "replay_gain_config.h"
#include "decoder_list.h"
#include "input_init.h"
#include "playlist_list.h"
#include "state_file.h"
#include "tag.h"
#include "dbUtils.h"
#include "zeroconf.h"
#include "event_pipe.h"
#include "tag_pool.h"
#include "mpd_error.h"

#ifdef ENABLE_INOTIFY
#include "inotify_update.h"
#endif

#ifdef ENABLE_SQLITE
#include "sticker.h"
#endif

#ifdef ENABLE_ARCHIVE
#include "archive_list.h"
#endif

#include <glib.h>

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

enum {
	DEFAULT_BUFFER_SIZE = 2048,
	DEFAULT_BUFFER_BEFORE_PLAY = 10,
};

GThread *main_task;
GMainLoop *main_loop;

GCond *main_cond;

struct player_control *global_player_control;

static bool
glue_daemonize_init(const struct options *options, GError **error_r)
{
	GError *error = NULL;

	char *pid_file = config_dup_path(CONF_PID_FILE, &error);
	if (pid_file == NULL && error != NULL) {
		g_propagate_error(error_r, error);
		return false;
	}

	daemonize_init(config_get_string(CONF_USER, NULL),
		       config_get_string(CONF_GROUP, NULL),
		       pid_file);
	g_free(pid_file);

	if (options->kill)
		daemonize_kill();

	return true;
}

static bool
glue_mapper_init(GError **error_r)
{
	GError *error = NULL;
	char *music_dir = config_dup_path(CONF_MUSIC_DIR, &error);
	if (music_dir == NULL && error != NULL) {
		g_propagate_error(error_r, error);
		return false;
	}

	char *playlist_dir = config_dup_path(CONF_PLAYLIST_DIR, &error);
	if (playlist_dir == NULL && error != NULL) {
		g_propagate_error(error_r, error);
		return false;
	}

	if (music_dir == NULL)
		music_dir = g_strdup(g_get_user_special_dir(G_USER_DIRECTORY_MUSIC));

	mapper_init(music_dir, playlist_dir);

	g_free(music_dir);
	g_free(playlist_dir);
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
	const struct config_param *path = config_get_param(CONF_DB_FILE);

	GError *error = NULL;
	bool ret;

	if (!mapper_has_music_directory()) {
		if (path != NULL)
			g_message("Found " CONF_DB_FILE " setting without "
				  CONF_MUSIC_DIR " - disabling database");
		db_init(NULL, NULL);
		return true;
	}

	if (path == NULL)
		MPD_ERROR(CONF_DB_FILE " setting missing");

	if (!db_init(path, &error))
		MPD_ERROR("%s", error->message);

	ret = db_load(&error);
	if (!ret)
		MPD_ERROR("%s", error->message);

	/* run database update after daemonization? */
	return db_exists();
}

/**
 * Configure and initialize the sticker subsystem.
 */
static void
glue_sticker_init(void)
{
#ifdef ENABLE_SQLITE
	GError *error = NULL;
	char *sticker_file = config_dup_path(CONF_STICKER_FILE, &error);
	if (sticker_file == NULL && error != NULL)
		MPD_ERROR("%s", error->message);

	if (!sticker_global_init(sticker_file, &error))
		MPD_ERROR("%s", error->message);

	g_free(sticker_file);
#endif
}

static bool
glue_state_file_init(GError **error_r)
{
	GError *error = NULL;

	char *path = config_dup_path(CONF_STATE_FILE, &error);
	if (path == NULL && error != NULL) {
		g_propagate_error(error_r, error);
		return false;
	}

	state_file_init(path, global_player_control);
	g_free(path);

	return true;
}

/**
 * Windows-only initialization of the Winsock2 library.
 */
static void winsock_init(void)
{
#ifdef WIN32
	WSADATA sockinfo;
	int retval;

	retval = WSAStartup(MAKEWORD(2, 2), &sockinfo);
	if(retval != 0)
	{
		MPD_ERROR("Attempt to open Winsock2 failed; error code %d\n",
			retval);
	}

	if (LOBYTE(sockinfo.wVersion) != 2)
	{
		MPD_ERROR("We use Winsock2 but your version is either too new "
			  "or old; please install Winsock 2.x\n");
	}
#endif
}

/**
 * Initialize the decoder and player core, including the music pipe.
 */
static void
initialize_decoder_and_player(void)
{
	const struct config_param *param;
	char *test;
	size_t buffer_size;
	float perc;
	unsigned buffered_chunks;
	unsigned buffered_before_play;

	param = config_get_param(CONF_AUDIO_BUFFER_SIZE);
	if (param != NULL) {
		long tmp = strtol(param->value, &test, 10);
		if (*test != '\0' || tmp <= 0 || tmp == LONG_MAX)
			MPD_ERROR("buffer size \"%s\" is not a positive integer, "
				  "line %i\n", param->value, param->line);
		buffer_size = tmp;
	} else
		buffer_size = DEFAULT_BUFFER_SIZE;

	buffer_size *= 1024;

	buffered_chunks = buffer_size / CHUNK_SIZE;

	if (buffered_chunks >= 1 << 15)
		MPD_ERROR("buffer size \"%li\" is too big\n", (long)buffer_size);

	param = config_get_param(CONF_BUFFER_BEFORE_PLAY);
	if (param != NULL) {
		perc = strtod(param->value, &test);
		if (*test != '%' || perc < 0 || perc > 100) {
			MPD_ERROR("buffered before play \"%s\" is not a positive "
				  "percentage and less than 100 percent, line %i",
				  param->value, param->line);
		}
	} else
		perc = DEFAULT_BUFFER_BEFORE_PLAY;

	buffered_before_play = (perc / 100) * buffered_chunks;
	if (buffered_before_play > buffered_chunks)
		buffered_before_play = buffered_chunks;

	global_player_control = pc_new(buffered_chunks, buffered_before_play);
}

/**
 * event_pipe callback function for PIPE_EVENT_IDLE
 */
static void
idle_event_emitted(void)
{
	/* send "idle" notificaions to all subscribed
	   clients */
	unsigned flags = idle_get();
	if (flags != 0)
		client_manager_idle_add(flags);
}

/**
 * event_pipe callback function for PIPE_EVENT_SHUTDOWN
 */
static void
shutdown_event_emitted(void)
{
	g_main_loop_quit(main_loop);
}

int main(int argc, char *argv[])
{
#ifdef WIN32
	return win32_main(argc, argv);
#else
	return mpd_main(argc, argv);
#endif
}

int mpd_main(int argc, char *argv[])
{
	struct options options;
	clock_t start;
	bool create_db;
	GError *error = NULL;
	bool success;

	daemonize_close_stdin();

#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	g_set_application_name("Music Player Daemon");

	/* enable GLib's thread safety code */
	g_thread_init(NULL);

	io_thread_init();
	winsock_init();
	idle_init();
	tag_pool_init();
	config_global_init();

	success = parse_cmdline(argc, argv, &options, &error);
	if (!success) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	if (!glue_daemonize_init(&options, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	stats_global_init();
	tag_lib_init();

	if (!log_init(options.verbose, options.log_stderr, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	success = listen_global_init(&error);
	if (!success) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	daemonize_set_user();

	main_task = g_thread_self();
	main_loop = g_main_loop_new(NULL, FALSE);
	main_cond = g_cond_new();

	event_pipe_init();
	event_pipe_register(PIPE_EVENT_IDLE, idle_event_emitted);
	event_pipe_register(PIPE_EVENT_SHUTDOWN, shutdown_event_emitted);

	path_global_init();

	if (!glue_mapper_init(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	initPermissions();
	playlist_global_init();
	spl_global_init();
#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif

	if (!pcm_resample_global_init(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	decoder_plugin_init_all();
	update_global_init();

	create_db = !glue_db_init_and_load();

	glue_sticker_init();

	command_init();
	initialize_decoder_and_player();
	volume_init();
	initAudioConfig();
	audio_output_all_init(global_player_control);
	client_manager_init();
	replay_gain_global_init();

	if (!input_stream_global_init(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	playlist_list_global_init();

	daemonize(options.daemon);

	setup_log_output(options.log_stderr);

	initSigHandlers();

	if (!io_thread_start(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	initZeroconf();

	player_create(global_player_control);

	if (create_db) {
		/* the database failed to load: recreate the
		   database */
		unsigned job = update_enqueue(NULL, true);
		if (job == 0)
			MPD_ERROR("directory update failed");
	}

	if (!glue_state_file_init(&error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	success = config_get_bool(CONF_AUTO_UPDATE, false);
#ifdef ENABLE_INOTIFY
	if (success && mapper_has_music_directory())
		mpd_inotify_init(config_get_unsigned(CONF_AUTO_UPDATE_DEPTH,
						     G_MAXUINT));
#else
	if (success)
		g_warning("inotify: auto_update was disabled. enable during compilation phase");
#endif

	config_global_check();

	/* enable all audio outputs (if not already done by
	   playlist_state_restore() */
	pc_update_audio(global_player_control);

#ifdef WIN32
	win32_app_started();
#endif

	/* run the main loop */
	g_main_loop_run(main_loop);

#ifdef WIN32
	win32_app_stopping();
#endif

	/* cleanup */

	g_main_loop_unref(main_loop);

#ifdef ENABLE_INOTIFY
	mpd_inotify_finish();
#endif

	state_file_finish(global_player_control);
	pc_kill(global_player_control);
	finishZeroconf();
	client_manager_deinit();
	listen_global_finish();
	playlist_global_finish();

	start = clock();
	db_finish();
	g_debug("db_finish took %f seconds",
		((float)(clock()-start))/CLOCKS_PER_SEC);

#ifdef ENABLE_SQLITE
	sticker_global_finish();
#endif

	g_cond_free(main_cond);
	event_pipe_deinit();

	playlist_list_global_finish();
	input_stream_global_finish();
	audio_output_all_finish();
	volume_finish();
	mapper_finish();
	path_global_finish();
	finishPermissions();
	pc_free(global_player_control);
	command_finish();
	update_global_finish();
	decoder_plugin_deinit_all();
#ifdef ENABLE_ARCHIVE
	archive_plugin_deinit_all();
#endif
	config_global_finish();
	tag_pool_deinit();
	idle_deinit();
	stats_global_finish();
	io_thread_deinit();
	daemonize_finish();
#ifdef WIN32
	WSACleanup();
#endif

	log_deinit();
	return EXIT_SUCCESS;
}
