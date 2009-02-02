/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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
#include "pipe.h"
#include "decoder_control.h"
#include "player_control.h"
#include "stats.h"
#include "sig_handlers.h"
#include "audio.h"
#include "volume.h"
#include "log.h"
#include "permission.h"
#include "replay_gain.h"
#include "decoder_list.h"
#include "input_stream.h"
#include "state_file.h"
#include "tag.h"
#include "dbUtils.h"
#include "config.h"
#include "normalize.h"
#include "zeroconf.h"
#include "event_pipe.h"
#include "dirvec.h"
#include "songvec.h"
#include "tag_pool.h"

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

enum {
	DEFAULT_BUFFER_SIZE = 2048,
	DEFAULT_BUFFER_BEFORE_PLAY = 10,
};

GThread *main_task;
GMainLoop *main_loop;

struct notify main_notify;

/**
 * Returns the database.  If this function returns false, this has not
 * succeeded, and the caller should create the database after the
 * process has been daemonized.
 */
static bool
openDB(const Options *options)
{
	const char *path = config_get_path(CONF_DB_FILE);
	bool ret;

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

	if (options->createDB > 0)
		/* don't attempt to load the old database */
		return false;

	ret = db_load();
	if (!ret) {
		if (options->createDB < 0)
			g_error("can't open db file and using "
				"\"--no-create-db\" command line option");

		if (!db_check())
			exit(EXIT_FAILURE);

		db_clear();

		/* run database update after daemonization */
		return false;
	}

	return true;
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
			FATAL("buffered before play \"%s\" is not a positive "
			      "percentage and less than 100 percent, line %i"
			      "\n", param->value, param->line);
		}
	} else
		perc = DEFAULT_BUFFER_BEFORE_PLAY;

	buffered_before_play = (perc / 100) * buffered_chunks;
	if (buffered_before_play > buffered_chunks)
		buffered_before_play = buffered_chunks;

	pc_init(buffered_before_play);
	music_pipe_init(buffered_chunks, &pc.notify);
	dc_init();
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
	Options options;
	clock_t start;
	bool create_db;

	daemonize_close_stdin();

#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	/* enable GLib's thread safety code */
	g_thread_init(NULL);

	idle_init();
	dirvec_init();
	songvec_init();
	tag_pool_init();
	config_global_init();

	parseOptions(argc, argv, &options);

	daemonize_init(config_get_string(CONF_USER, NULL),
		       config_get_path(CONF_PID_FILE));

	if (options.kill)
		daemonize_kill();

	stats_global_init();
	tag_lib_init();
	log_init(options.verbose, options.stdOutput);

	listenOnPort();

	daemonize_set_user();

	main_task = g_thread_self();
	main_loop = g_main_loop_new(NULL, FALSE);
	notify_init(&main_notify);

	event_pipe_init();
	event_pipe_register(PIPE_EVENT_IDLE, idle_event_emitted);
	event_pipe_register(PIPE_EVENT_PLAYLIST, syncPlayerAndPlaylist);

	path_global_init();
	mapper_init();
	initPermissions();
	initPlaylist();
	spl_global_init();
#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif
	decoder_plugin_init_all();
	update_global_init();

	create_db = !openDB(&options);

#ifdef ENABLE_SQLITE
	sticker_global_init(config_get_path(CONF_STICKER_FILE));
#endif

	command_init();
	initialize_decoder_and_player();
	volume_init();
	initAudioConfig();
	initAudioDriver();
	client_manager_init();
	replay_gain_global_init();
	initNormalization();
	input_stream_global_init();

	daemonize(options.daemon);

	setup_log_output(options.stdOutput);

	initSigHandlers();

	initZeroconf();

	player_create();

	if (create_db) {
		/* the database failed to load, or MPD was started
		   with --create-db: recreate a new database */
		unsigned job = directory_update_init(NULL);
		if (job == 0)
			g_error("directory update failed");
	}


	state_file_init(config_get_path(CONF_STATE_FILE));

	/* run the main loop */

	g_main_loop_run(main_loop);

	/* cleanup */

	g_main_loop_unref(main_loop);

	state_file_finish();
	playerKill();
	finishZeroconf();
	client_manager_deinit();
	closeAllListenSockets();
	finishPlaylist();

	start = clock();
	db_finish();
	g_debug("db_finish took %f seconds",
		((float)(clock()-start))/CLOCKS_PER_SEC);

#ifdef ENABLE_SQLITE
	sticker_global_finish();
#endif

	notify_deinit(&main_notify);
	event_pipe_deinit();

	input_stream_global_finish();
	finishNormalization();
	finishAudioDriver();
	finishAudioConfig();
	volume_finish();
	mapper_finish();
	path_global_finish();
	finishPermissions();
	dc_deinit();
	pc_deinit();
	command_finish();
	update_global_finish();
	decoder_plugin_deinit_all();
#ifdef ENABLE_ARCHIVE
	archive_plugin_deinit_all();
#endif
	music_pipe_free();
	config_global_finish();
	tag_pool_deinit();
	songvec_deinit();
	dirvec_deinit();
	idle_deinit();
	stats_global_finish();
	daemonize_finish();

	close_log_files();
	return EXIT_SUCCESS;
}
