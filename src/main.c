/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "client.h"
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
#include "audio.h"
#include "output_all.h"
#include "volume.h"
#include "log.h"
#include "permission.h"
#include "replay_gain.h"
#include "decoder_list.h"
#include "input_stream.h"
#include "playlist_list.h"
#include "state_file.h"
#include "tag.h"
#include "dbUtils.h"
#include "normalize.h"
#include "zeroconf.h"
#include "event_pipe.h"
#include "dirvec.h"
#include "songvec.h"
#include "tag_pool.h"

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

static void
glue_daemonize_init(const struct options *options)
{
	daemonize_init(config_get_string(CONF_USER, NULL),
		       config_get_string(CONF_GROUP, NULL),
		       config_get_path(CONF_PID_FILE));

	if (options->kill)
		daemonize_kill();
}

static void
glue_mapper_init(void)
{
	const char *music_dir, *playlist_dir;

	music_dir = config_get_path(CONF_MUSIC_DIR);
#if GLIB_CHECK_VERSION(2,14,0)
	if (music_dir == NULL)
		music_dir = g_get_user_special_dir(G_USER_DIRECTORY_MUSIC);
#endif

	playlist_dir = config_get_path(CONF_PLAYLIST_DIR);

	mapper_init(music_dir, playlist_dir);
}

/**
 * Returns the database.  If this function returns false, this has not
 * succeeded, and the caller should create the database after the
 * process has been daemonized.
 */
static bool
glue_db_init_and_load(void)
{
	const char *path = config_get_path(CONF_DB_FILE);
	bool ret;
	GError *error = NULL;

	if (!mapper_has_music_directory()) {
		if (path != NULL)
			g_message("Found " CONF_DB_FILE " setting without "
				  CONF_MUSIC_DIR " - disabling database");
		db_init(NULL);
		return true;
	}

	if (path == NULL)
		g_error(CONF_DB_FILE " setting missing");

	db_init(path);

	ret = db_load(&error);
	if (!ret) {
		g_warning("Failed to load database: %s", error->message);
		g_error_free(error);

		if (!db_check())
			exit(EXIT_FAILURE);

		db_clear();

		/* run database update after daemonization */
		return false;
	}

	return true;
}

/**
 * Configure and initialize the sticker subsystem.
 */
static void
glue_sticker_init(void)
{
#ifdef ENABLE_SQLITE
	bool success;
	GError *error = NULL;

	success = sticker_global_init(config_get_path(CONF_STICKER_FILE),
				      &error);
	if (!success)
		g_error("%s", error->message);
#endif
}

static void
glue_state_file_init(void)
{
	state_file_init(config_get_path(CONF_STATE_FILE));
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
		g_error("Attempt to open Winsock2 failed; error code %d\n",
			retval);
	}

	if (LOBYTE(sockinfo.wVersion) != 2)
	{
		g_error("We use Winsock2 but your version is either too new or "
			"old; please install Winsock 2.x\n");
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
		buffer_size = strtol(param->value, &test, 10);
		if (*test != '\0' || buffer_size <= 0)
			g_error("buffer size \"%s\" is not a positive integer, "
				"line %i\n", param->value, param->line);
	} else
		buffer_size = DEFAULT_BUFFER_SIZE;

	buffer_size *= 1024;

	buffered_chunks = buffer_size / CHUNK_SIZE;

	if (buffered_chunks >= 1 << 15)
		g_error("buffer size \"%li\" is too big\n", (long)buffer_size);

	param = config_get_param(CONF_BUFFER_BEFORE_PLAY);
	if (param != NULL) {
		perc = strtod(param->value, &test);
		if (*test != '%' || perc < 0 || perc > 100) {
			g_error("buffered before play \"%s\" is not a positive "
				"percentage and less than 100 percent, line %i",
				param->value, param->line);
		}
	} else
		perc = DEFAULT_BUFFER_BEFORE_PLAY;

	buffered_before_play = (perc / 100) * buffered_chunks;
	if (buffered_before_play > buffered_chunks)
		buffered_before_play = buffered_chunks;

	pc_init(buffered_chunks, buffered_before_play);
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

int main(int argc, char *argv[])
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

	winsock_init();
	idle_init();
	dirvec_init();
	songvec_init();
	tag_pool_init();
	config_global_init();

	success = parse_cmdline(argc, argv, &options, &error);
	if (!success) {
		g_warning("%s", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	glue_daemonize_init(&options);

	stats_global_init();
	tag_lib_init();
	log_init(options.verbose, options.log_stderr);

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

	path_global_init();
	glue_mapper_init();
	initPermissions();
	playlist_global_init();
	spl_global_init();
#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif
	decoder_plugin_init_all();
	update_global_init();

	create_db = !glue_db_init_and_load();

	glue_sticker_init();

	command_init();
	initialize_decoder_and_player();
	volume_init();
	initAudioConfig();
	audio_output_all_init();
	client_manager_init();
	replay_gain_global_init();
	initNormalization();
	input_stream_global_init();
	playlist_list_global_init();

	daemonize(options.daemon);

	setup_log_output(options.log_stderr);

	initSigHandlers();

	initZeroconf();

	player_create();

	if (create_db) {
		/* the database failed to load: recreate the
		   database */
		unsigned job = update_enqueue(NULL, true);
		if (job == 0)
			g_error("directory update failed");
	}

	glue_state_file_init();

	success = config_get_bool(CONF_AUTO_UPDATE, false);
#ifdef ENABLE_INOTIFY
	if (success && mapper_has_music_directory())
    		mpd_inotify_init();
#else
	if (success)
		g_warning("inotify: auto_update was disabled. enable during compilation phase");
#endif

	config_global_check();

	/* enable all audio outputs (if not already done by
	   playlist_state_restore() */
	pc_update_audio();

	/* run the main loop */

	g_main_loop_run(main_loop);

	/* cleanup */

	g_main_loop_unref(main_loop);

#ifdef ENABLE_INOTIFY
	mpd_inotify_finish();
#endif

	state_file_finish();
	pc_kill();
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
	finishNormalization();
	audio_output_all_finish();
	volume_finish();
	mapper_finish();
	path_global_finish();
	finishPermissions();
	pc_deinit();
	command_finish();
	update_global_finish();
	decoder_plugin_deinit_all();
#ifdef ENABLE_ARCHIVE
	archive_plugin_deinit_all();
#endif
	config_global_finish();
	tag_pool_deinit();
	songvec_deinit();
	dirvec_deinit();
	idle_deinit();
	stats_global_finish();
	daemonize_finish();
#ifdef WIN32
	WSACleanup();
#endif

	close_log_files();
	return EXIT_SUCCESS;
}
