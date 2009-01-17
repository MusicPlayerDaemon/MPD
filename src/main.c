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
#include "database.h"
#include "update.h"
#include "player_thread.h"
#include "listen.h"
#include "cmdline.h"
#include "conf.h"
#include "path.h"
#include "mapper.h"
#include "playerData.h"
#include "pipe.h"
#include "decoder_thread.h"
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

#ifdef ENABLE_ARCHIVE
#include "archive_list.h"
#endif

#include <glib.h>

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#ifndef WIN32
#include <pwd.h>
#include <grp.h>
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

GThread *main_task;
GMainLoop *main_loop;

struct notify main_notify;

static void changeToUser(void)
{
#ifndef WIN32
	struct config_param *param = config_get_param(CONF_USER);

	if (param && strlen(param->value)) {
		/* get uid */
		struct passwd *userpwd;
		if ((userpwd = getpwnam(param->value)) == NULL) {
			g_error("no such user \"%s\" at line %i",
				param->value, param->line);
		}

		if (setgid(userpwd->pw_gid) == -1) {
			g_error("cannot setgid for user \"%s\" at line %i: %s",
				param->value, param->line, strerror(errno));
		}
#ifdef _BSD_SOURCE
		/* init suplementary groups 
		 * (must be done before we change our uid)
		 */
		if (initgroups(param->value, userpwd->pw_gid) == -1) {
			g_warning("cannot init supplementary groups "
				  "of user \"%s\" at line %i: %s",
				  param->value, param->line, strerror(errno));
		}
#endif

		/* set uid */
		if (setuid(userpwd->pw_uid) == -1) {
			g_error("cannot change to uid of user "
				"\"%s\" at line %i: %s",
				param->value, param->line, strerror(errno));
		}

		/* this is needed by libs such as arts */
		if (userpwd->pw_dir) {
			g_setenv("HOME", userpwd->pw_dir, true);
		}
	}
#endif
}

static void openDB(Options * options, char *argv0)
{
	db_init();

	if (options->createDB > 0 || !db_load()) {
		unsigned job;

		if (options->createDB < 0) {
			g_error("can't open db file and using "
				"\"--no-create-db\" command line option; "
				"try running \"%s --create-db\"", argv0);
		}

		if (!db_check())
			exit(EXIT_FAILURE);

		db_clear();

		job = directory_update_init(NULL);
		if (job == 0)
			g_error("directory update failed");
	}
}

static void cleanUpPidFile(void)
{
	struct config_param *pidFileParam =
		parseConfigFilePath(CONF_PID_FILE, 0);

	if (!pidFileParam)
		return;

	g_debug("cleaning up pid file");

	unlink(pidFileParam->value);
}

static void killFromPidFile(void)
{
#ifndef WIN32
	FILE *fp;
	struct config_param *pidFileParam =
		parseConfigFilePath(CONF_PID_FILE, 0);
	int pid;

	if (!pidFileParam) {
		g_error("no pid_file specified in the config file");
	}

	fp = fopen(pidFileParam->value, "r");
	if (!fp) {
		g_error("unable to open %s \"%s\": %s",
			CONF_PID_FILE, pidFileParam->value, strerror(errno));
	}
	if (fscanf(fp, "%i", &pid) != 1) {
		g_error("unable to read the pid from file \"%s\"",
			pidFileParam->value);
	}
	fclose(fp);

	if (kill(pid, SIGTERM)) {
		g_error("unable to kill proccess %i: %s",
			pid, strerror(errno));
	}
	exit(EXIT_SUCCESS);
#else
	g_error("--kill is not available on WIN32");
#endif
}

static gboolean
timer_save_state_file(G_GNUC_UNUSED gpointer data)
{
	g_debug("Saving state file");
	write_state_file();
	return true;
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
	GTimer *save_state_timer;
	guint save_state_source_id;

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

	if (options.kill)
		killFromPidFile();

	initStats();
	tag_lib_init();
	log_init(options.verbose, options.stdOutput);

	listenOnPort();

	changeToUser();

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
#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif
	decoder_plugin_init_all();
	update_global_init();

	openDB(&options, argv[0]);

	command_init();
	initPlayerData();
	pc_init(buffered_before_play);
	music_pipe_init(buffered_chunks, &pc.notify);
	dc_init();
	initAudioConfig();
	initAudioDriver();
	volume_init();
	client_manager_init();
	replay_gain_global_init();
	initNormalization();
	input_stream_global_init();

	daemonize(&options);

	setup_log_output(options.stdOutput);

	initSigHandlers();

	initZeroconf();

	decoder_thread_start();
	player_create();
	read_state_file();

	save_state_timer = g_timer_new();

	save_state_source_id = g_timeout_add(5 * 60 * 1000,
					     timer_save_state_file, NULL);

	/* run the main loop */

	g_main_loop_run(main_loop);

	/* cleanup */

	g_main_loop_unref(main_loop);

	g_source_remove(save_state_source_id);
	g_timer_destroy(save_state_timer);

	write_state_file();
	playerKill();
	finishZeroconf();
	client_manager_deinit();
	closeAllListenSockets();
	finishPlaylist();

	start = clock();
	db_finish();
	g_debug("db_finish took %f seconds",
		((float)(clock()-start))/CLOCKS_PER_SEC);

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
	cleanUpPidFile();
	config_global_finish();
	tag_pool_deinit();
	songvec_deinit();
	dirvec_deinit();
	idle_deinit();

	close_log_files();
	return EXIT_SUCCESS;
}
