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
#include "../config.h"
#include "normalize.h"
#include "zeroconf.h"
#include "main_notify.h"
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

#ifdef HAVE_LOCALE
#include <locale.h>
#endif

static void changeToUser(void)
{
#ifndef WIN32
	ConfigParam *param = getConfigParam(CONF_USER);

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
	if (options->createDB > 0 || db_load() < 0) {
		if (options->createDB < 0) {
			g_error("can't open db file and using "
				"\"--no-create-db\" command line option; "
				"try running \"%s --create-db\"", argv0);
		}
		if (db_check() < 0)
			exit(EXIT_FAILURE);
		db_init();
		if (db_save() < 0)
			exit(EXIT_FAILURE);
		if (options->createDB)
			exit(EXIT_SUCCESS);
	}
}

static void cleanUpPidFile(void)
{
	ConfigParam *pidFileParam = parseConfigFilePath(CONF_PID_FILE, 0);

	if (!pidFileParam)
		return;

	g_debug("cleaning up pid file");

	unlink(pidFileParam->value);
}

static void killFromPidFile(void)
{
#ifndef WIN32
	FILE *fp;
	ConfigParam *pidFileParam = parseConfigFilePath(CONF_PID_FILE, 0);
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

int main(int argc, char *argv[])
{
	Options options;
	clock_t start;
	GTimer *save_state_timer;

#ifdef HAVE_LOCALE
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	/* enable GLib's thread safety code */
	g_thread_init(NULL);

	idle_init();
	dirvec_init();
	songvec_init();
	tag_pool_init();
	initConf();

	parseOptions(argc, argv, &options);

	if (options.kill)
		killFromPidFile();

	initStats();
	tag_lib_init();
	log_init(options.verbose, options.stdOutput);

	if (options.createDB <= 0)
		listenOnPort();

	changeToUser();

	path_global_init();
	mapper_init();
	initPermissions();
	initPlaylist();
#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif
	decoder_plugin_init_all();
	update_global_init();

	init_main_notify();

	openDB(&options, argv[0]);

	command_init();
	initPlayerData();
	pc_init(buffered_before_play);
	music_pipe_init(buffered_chunks, &pc.notify);
	dc_init();
	initAudioConfig();
	initAudioDriver();
	initVolume();
	client_manager_init();
	replay_gain_global_init();
	initNormalization();
	input_stream_global_init();

	daemonize(&options);

	setup_log_output(options.stdOutput);

	initSigHandlers();

	initZeroconf();

	openVolumeDevice();
	decoder_thread_start();
	player_create();
	read_state_file();

	save_state_timer = g_timer_new();

	while (COMMAND_RETURN_KILL != client_manager_io() &&
	       COMMAND_RETURN_KILL != handlePendingSignals()) {
		unsigned flags;

		syncPlayerAndPlaylist();
		client_manager_expire();
		reap_update_task();

		/* send "idle" notificaions to all subscribed
		   clients */
		flags = idle_get();
		if (flags != 0)
			client_manager_idle_add(flags);

		if (g_timer_elapsed(save_state_timer, NULL) >= 5 * 60) {
			g_debug("Saving state file");
			write_state_file();
			g_timer_start(save_state_timer);
		}
	}

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

	deinit_main_notify();

	input_stream_global_finish();
	finishNormalization();
	finishAudioDriver();
	finishAudioConfig();
	finishVolume();
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
	finishConf();
	tag_pool_deinit();
	songvec_deinit();
	dirvec_deinit();
	idle_deinit();

	close_log_files();
	return EXIT_SUCCESS;
}
