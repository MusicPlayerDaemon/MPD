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

#include "interface.h"
#include "command.h"
#include "playlist.h"
#include "directory.h"
#include "player.h"
#include "listen.h"
#include "conf.h"
#include "path.h"
#include "playerData.h"
#include "stats.h"
#include "sig_handlers.h"
#include "audio.h"
#include "volume.h"
#include "log.h"
#include "permission.h"
#include "replayGain.h"
#include "inputPlugin.h"
#include "audioOutput.h"
#include "inputStream.h"
#include "state_file.h"
#include "tag.h"
#include "tagTracker.h"
#include "dbUtils.h"
#include "../config.h"
#include "utils.h"
#include "normalize.h"
#include "zeroconf.h"

#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>

#define SYSTEM_CONFIG_FILE_LOCATION	"/etc/mpd.conf"
#define USER_CONFIG_FILE_LOCATION	"/.mpdconf"

typedef struct _Options {
	int kill;
	int daemon;
	int stdOutput;
	int createDB;
	int verbose;
} Options;

/* 
 * from git-1.3.0, needed for solaris
 */
#ifndef HAVE_SETENV
static int setenv(const char *name, const char *value, int replace)
{
	int out;
	size_t namelen, valuelen;
	char *envstr;

	if (!name || !value)
		return -1;
	if (!replace) {
		char *oldval = NULL;
		oldval = getenv(name);
		if (oldval)
			return 0;
	}

	namelen = strlen(name);
	valuelen = strlen(value);
	envstr = xmalloc((namelen + valuelen + 2));
	if (!envstr)
		return -1;

	memcpy(envstr, name, namelen);
	envstr[namelen] = '=';
	memcpy(envstr + namelen + 1, value, valuelen);
	envstr[namelen + valuelen + 1] = 0;

	out = putenv(envstr);
	/* putenv(3) makes the argument string part of the environment,
	 * and changing that string modifies the environment --- which
	 * means we do not own that storage anymore.  Do not free
	 * envstr.
	 */

	return out;
}
#endif /* HAVE_SETENV */

static void usage(char *argv[])
{
	ERROR("usage:\n");
	ERROR("   %s [options] <conf file>\n", argv[0]);
	ERROR("   %s [options]   (searches for ~%s then %s)\n",
	      argv[0], USER_CONFIG_FILE_LOCATION, SYSTEM_CONFIG_FILE_LOCATION);
	ERROR("\n");
	ERROR("options:\n");
	ERROR("   --help             this usage statement\n");
	ERROR("   --kill             kill the currently running mpd session\n");
	ERROR
	    ("   --create-db        force (re)creation of database and exit\n");
	ERROR
	    ("   --no-create-db     don't create database, even if it doesn't exist\n");
	ERROR("   --no-daemon        don't detach from console\n");
	ERROR("   --stdout           print messages to stdout and stderr\n");
	ERROR("   --verbose          verbose logging\n");
	ERROR("   --version          prints version information\n");
}

static void version(void)
{
	LOG("mpd (MPD: Music Player Daemon) %s\n", VERSION);
	LOG("\n");
	LOG("Copyright (C) 2003-2007 Warren Dukes <warren.dukes@gmail.com>\n");
	LOG("This is free software; see the source for copying conditions.  There is NO\n");
	LOG("warranty; not even MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	LOG("\n");
	LOG("Supported formats:\n");

	initInputPlugins();
	printAllInputPluginSuffixes(stdout);

	LOG("\n");
	LOG("Supported outputs:\n");
	loadAudioDrivers();
	printAllOutputPluginTypes(stdout);
}

static void parseOptions(int argc, char **argv, Options * options)
{
	int argcLeft = argc;

	options->verbose = 0;
	options->daemon = 1;
	options->stdOutput = 0;
	options->createDB = 0;
	options->kill = 0;

	if (argc > 1) {
		int i = 1;
		while (i < argc) {
			if (strncmp(argv[i], "--", 2) == 0) {
				if (strcmp(argv[i], "--help") == 0) {
					usage(argv);
					exit(EXIT_SUCCESS);
				} else if (strcmp(argv[i], "--kill") == 0) {
					options->kill++;
					argcLeft--;
				} else if (strcmp(argv[i], "--no-daemon") == 0) {
					options->daemon = 0;
					argcLeft--;
				} else if (strcmp(argv[i], "--stdout") == 0) {
					options->stdOutput = 1;
					argcLeft--;
				} else if (strcmp(argv[i], "--create-db") == 0) {
					options->stdOutput = 1;
					options->createDB = 1;
					argcLeft--;
				} else if (strcmp(argv[i], "--no-create-db") ==
					   0) {
					options->createDB = -1;
					argcLeft--;
				} else if (strcmp(argv[i], "--verbose") == 0) {
					options->verbose = 1;
					argcLeft--;
				} else if (strcmp(argv[i], "--version") == 0) {
					version();
					exit(EXIT_SUCCESS);
				} else {
					fprintf(stderr,
					          "unknown command line option: %s\n",
						  argv[i]);
					exit(EXIT_FAILURE);
				}
			} else
				break;
			i++;
		}
	}

	if (argcLeft <= 2) {
		if (argcLeft == 2) {
			readConf(argv[argc - 1]);
			return;
		} else if (argcLeft == 1) {
			struct stat st;
			char *homedir = getenv("HOME");
			char userfile[MAXPATHLEN + 1] = "";
			if (homedir && (strlen(homedir) +
					strlen(USER_CONFIG_FILE_LOCATION)) <
			    MAXPATHLEN) {
				strcpy(userfile, homedir);
				strcat(userfile, USER_CONFIG_FILE_LOCATION);
			}
			if (strlen(userfile) && (0 == stat(userfile, &st))) {
				readConf(userfile);
				return;
			} else if (0 == stat(SYSTEM_CONFIG_FILE_LOCATION, &st)) {
				readConf(SYSTEM_CONFIG_FILE_LOCATION);
				return;
			}
		}
	}

	usage(argv);
	exit(EXIT_FAILURE);
}

static void closeAllFDs(void)
{
	int i;
	int fds = getdtablesize();

	for (i = 3; i < fds; i++)
		close(i);
}

static void changeToUser(void)
{
	ConfigParam *param = getConfigParam(CONF_USER);

	if (param && strlen(param->value)) {
		/* get uid */
		struct passwd *userpwd;
		if ((userpwd = getpwnam(param->value)) == NULL) {
			FATAL("no such user \"%s\" at line %i\n", param->value,
			      param->line);
		}

		if (setgid(userpwd->pw_gid) == -1) {
			FATAL("cannot setgid for user \"%s\" at line %i: %s\n",
			      param->value, param->line, strerror(errno));
		}
#ifdef _BSD_SOURCE
		/* init suplementary groups 
		 * (must be done before we change our uid)
		 */
		if (initgroups(param->value, userpwd->pw_gid) == -1) {
			WARNING("cannot init supplementary groups "
				"of user \"%s\" at line %i: %s\n",
				param->value, param->line, strerror(errno));
		}
#endif

		/* set uid */
		if (setuid(userpwd->pw_uid) == -1) {
			FATAL("cannot change to uid of user "
			      "\"%s\" at line %i: %s\n",
			      param->value, param->line, strerror(errno));
		}

		/* this is needed by libs such as arts */
		if (userpwd->pw_dir) {
			setenv("HOME", userpwd->pw_dir, 1);
		}
	}
}

static void openDB(Options * options, char *argv0)
{
	if (options->createDB > 0 || readDirectoryDB() < 0) {
		if (options->createDB < 0) {
			FATAL("can't open db file and using "
			      "\"--no-create-db\" command line option\n"
			      "try running \"%s --create-db\"\n", argv0);
		}
		flushWarningLog();
		if (checkDirectoryDB() < 0)
			exit(EXIT_FAILURE);
		initMp3Directory();
		if (writeDirectoryDB() < 0)
			exit(EXIT_FAILURE);
		if (options->createDB)
			exit(EXIT_SUCCESS);
	}
}

static void daemonize(Options * options)
{
	FILE *fp = NULL;
	ConfigParam *pidFileParam = parseConfigFilePath(CONF_PID_FILE, 0);

	if (pidFileParam) {
		/* do this before daemon'izing so we can fail gracefully if we can't
		 * write to the pid file */
		DEBUG("opening pid file\n");
		fp = fopen(pidFileParam->value, "w+");
		if (!fp) {
			FATAL("could not open %s \"%s\" (at line %i) for writing: %s\n",
			     CONF_PID_FILE, pidFileParam->value,
			     pidFileParam->line, strerror(errno));
		}
	}

	if (options->daemon) {
		int pid;

		fflush(NULL);
		pid = fork();
		if (pid > 0)
			_exit(EXIT_SUCCESS);
		else if (pid < 0) {
			FATAL("problems fork'ing for daemon!\n");
		}

		if (chdir("/") < 0) {
			FATAL("problems changing to root directory\n");
		}

		if (setsid() < 0) {
			FATAL("problems setsid'ing\n");
		}

		fflush(NULL);
		pid = fork();
		if (pid > 0)
			_exit(EXIT_SUCCESS);
		else if (pid < 0) {
			FATAL("problems fork'ing for daemon!\n");
		}

		DEBUG("daemonized!\n");
	}

	if (pidFileParam) {
		DEBUG("writing pid file\n");
		fprintf(fp, "%lu\n", (unsigned long)getpid());
		fclose(fp);
	}
}

static void cleanUpPidFile(void)
{
	ConfigParam *pidFileParam = parseConfigFilePath(CONF_PID_FILE, 0);

	if (!pidFileParam)
		return;

	DEBUG("cleaning up pid file\n");

	unlink(pidFileParam->value);
}

static void killFromPidFile(char *cmd, int killOption)
{
	FILE *fp;
	ConfigParam *pidFileParam = parseConfigFilePath(CONF_PID_FILE, 0);
	int pid;

	if (!pidFileParam) {
		FATAL("no pid_file specified in the config file\n");
	}

	fp = fopen(pidFileParam->value, "r");
	if (!fp) {
		FATAL("unable to open %s \"%s\": %s\n",
		      CONF_PID_FILE, pidFileParam->value, strerror(errno));
	}
	if (fscanf(fp, "%i", &pid) != 1) {
		FATAL("unable to read the pid from file \"%s\"\n",
		      pidFileParam->value);
	}
	fclose(fp);

	if (kill(pid, SIGTERM)) {
		FATAL("unable to kill proccess %i: %s\n", pid, strerror(errno));
	}
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	Options options;
	clock_t start;

	closeAllFDs();

	initConf();

	parseOptions(argc, argv, &options);

	if (options.kill)
		killFromPidFile(argv[0], options.kill);

	initStats();
	initTagConfig();
	initLog(options.verbose);

	if (options.createDB <= 0)
		listenOnPort();

	changeToUser();

	open_log_files(options.stdOutput);

	initPaths();
	initPermissions();
	initPlaylist();
	initInputPlugins();

	openDB(&options, argv[0]);

	initCommands();
	initPlayerData();
	initAudioConfig();
	initAudioDriver();
	initVolume();
	initInterfaces();
	initReplayGainState();
	initNormalization();
	initInputStream();

	daemonize(&options);

	setup_log_output(options.stdOutput);

	initSigHandlers();

	initZeroconf();

	openVolumeDevice();
	read_state_file();

	while (COMMAND_RETURN_KILL != doIOForInterfaces()) {
		if (COMMAND_RETURN_KILL == handlePendingSignals())
			break;
		syncPlayerAndPlaylist();
		closeOldInterfaces();
		readDirectoryDBIfUpdateIsFinished();
	}

	write_state_file();
	playerKill();
	finishZeroconf();
	freeAllInterfaces();
	closeAllListenSockets();
	finishPlaylist();

	start = clock();
	closeMp3Directory();
	DEBUG("closeMp3Directory took %f seconds\n", 
	      ((float)(clock()-start))/CLOCKS_PER_SEC);

	finishNormalization();
	finishAudioDriver();
	finishAudioConfig();
	finishVolume();
	finishPaths();
	finishPermissions();
	finishCommands();
	finishInputPlugins();
	cleanUpPidFile();
	finishConf();
	freePlayerData();

	close_log_files();
	return EXIT_SUCCESS;
}
