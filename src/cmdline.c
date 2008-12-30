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

#include <stdio.h>
#include <stdlib.h>

#define SYSTEM_CONFIG_FILE_LOCATION	"/etc/mpd.conf"
#define USER_CONFIG_FILE_LOCATION	".mpdconf"

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
	     "Supported decoders:\n");
	decoder_plugin_print_all_decoders(stdout);

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

#if GLIB_MAJOR_VERSION > 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 12)
static const char *summary =
	"Music Player Daemon - a daemon for playing music.";
#endif

void parseOptions(int argc, char **argv, Options *options)
{
	GError *error = NULL;
	GOptionContext *context;
	bool ret;
	static gboolean option_version,
		option_create_db, option_no_create_db, option_no_daemon;
	const GOptionEntry entries[] = {
		{ "version", 'V', 0, G_OPTION_ARG_NONE, &option_version,
		  "print version number", NULL },
		{ "kill", 0, 0, G_OPTION_ARG_NONE, &options->kill,
		  "kill the currently running mpd session", NULL },
		{ "create-db", 0, 0, G_OPTION_ARG_NONE, &option_create_db,
		  "force (re)creation of database and exit", NULL },
		{ "no-create-db", 0, 0, G_OPTION_ARG_NONE, &option_no_create_db,
		  "don't create database, even if it doesn't exist", NULL },
		{ "no-daemon", 0, 0, G_OPTION_ARG_NONE, &option_no_daemon,
		  "don't detach from console", NULL },
		{ "stdout", 0, 0, G_OPTION_ARG_NONE, &options->stdOutput,
		  "print messages to stderr", NULL },
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &options->verbose,
		  "verbose logging", NULL },
		{ .long_name = NULL }
	};

	options->kill = false;
	options->daemon = true;
	options->stdOutput = false;
	options->verbose = false;
	options->createDB = 0;

	context = g_option_context_new("[path/to/mpd.conf]");
	g_option_context_add_main_entries(context, entries, NULL);

#if GLIB_MAJOR_VERSION > 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 12)
	g_option_context_set_summary(context, summary);
#endif

	ret = g_option_context_parse(context, &argc, &argv, &error);
	g_option_context_free(context);

	if (!ret) {
		g_error("option parsing failed: %s\n", error->message);
		exit(1);
	}

	if (option_version)
		version();

	if (option_create_db && option_no_create_db)
		g_error("Cannot use both --create-db and --no-create-db\n");

	if (option_no_create_db)
		options->createDB = -1;
	else if (option_create_db)
		options->createDB = 1;

	options->daemon = !option_no_daemon;

	if (argc <= 1) {
		/* default configuration file path */
		char *path;

		path = g_build_filename(g_get_home_dir(),
					USER_CONFIG_FILE_LOCATION, NULL);
		if (g_file_test(path, G_FILE_TEST_IS_REGULAR))
			readConf(path);
		else if (g_file_test(SYSTEM_CONFIG_FILE_LOCATION,
				     G_FILE_TEST_IS_REGULAR))
			readConf(SYSTEM_CONFIG_FILE_LOCATION);
		g_free(path);
	} else if (argc == 2) {
		/* specified configuration file */
		readConf(argv[1]);
	} else
		g_error("too many arguments");
}
