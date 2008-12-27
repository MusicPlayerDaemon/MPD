/*
 * Copyright (C) 2003-2008 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "cmdline.h"
#include "path.h"
#include "conf.h"
#include "decoder_list.h"
#include "config.h"
#include "audioOutput.h"

#ifdef ENABLE_ARCHIVE
#include "archive_list.h"
#endif

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSTEM_CONFIG_FILE_LOCATION	"/etc/mpd.conf"
#define USER_CONFIG_FILE_LOCATION	"/.mpdconf"

static void usage(char *argv[])
{
	printf("usage:\n"
	       "   %s [options] <conf file>\n"
	       "   %s [options]   (searches for ~" USER_CONFIG_FILE_LOCATION
	       " then " SYSTEM_CONFIG_FILE_LOCATION ")\n",
	       argv[0], argv[0]);
	puts("\n"
	     "options:\n"
	     "   --help             this usage statement\n"
	     "   --kill             kill the currently running mpd session\n"
	     "   --create-db        force (re)creation of database and exit\n"
	     "   --no-create-db     don't create database, even if it doesn't exist\n"
	     "   --no-daemon        don't detach from console\n"
	     "   --stdout           print messages to stdout and stderr\n"
	     "   --verbose          verbose logging\n"
	     "   --version          prints version information\n");
}

static void version(void)
{
	puts(PACKAGE " (MPD: Music Player Daemon) " VERSION " \n"
	     "\n"
	     "Copyright (C) 2003-2007 Warren Dukes <warren.dukes@gmail.com>\n"
	     "Copyright (C) 2008 Max Kellermann <max@duempel.org>\n"
	     "This is free software; see the source for copying conditions.  There is NO\n"
	     "warranty; not even MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
	     "\n"
	     "Supported formats:\n");

	decoder_plugin_init_all();
	decoder_plugin_print_all_suffixes(stdout);

	puts("\n"
	     "Supported outputs:\n");
	printAllOutputPluginTypes(stdout);

#ifdef ENABLE_ARCHIVE
	puts("\n"
	     "Supported archives:\n");
	archive_plugin_init_all();
	archive_plugin_print_all_suffixes(stdout);
#endif
}

void parseOptions(int argc, char **argv, Options *options)
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
			if (g_str_has_prefix(argv[i], "--")) {
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
			char userfile[MPD_PATH_MAX] = "";
			if (homedir && (strlen(homedir) +
					strlen(USER_CONFIG_FILE_LOCATION)) <
			    MPD_PATH_MAX) {
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
