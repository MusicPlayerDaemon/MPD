/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "DatabaseRegistry.hxx"
#include "DatabasePlugin.hxx"
#include "DatabaseSelection.hxx"
#include "directory.h"
#include "song.h"
#include "playlist_vector.h"

extern "C" {
#include "conf.h"
#include "tag.h"
}

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <stdlib.h>

static void
my_log_func(const gchar *log_domain, G_GNUC_UNUSED GLogLevelFlags log_level,
	    const gchar *message, G_GNUC_UNUSED gpointer user_data)
{
	if (log_domain != NULL)
		g_printerr("%s: %s\n", log_domain, message);
	else
		g_printerr("%s\n", message);
}

static bool
DumpDirectory(const directory &directory, GError **)
{
	cout << "D " << directory.path << endl;
	return true;
}

static bool
DumpSong(song &song, GError **)
{
	cout << "S " << song.parent->path << "/" << song.uri << endl;
	return true;
}

static bool
DumpPlaylist(const playlist_metadata &playlist,
	     const directory &directory, GError **)
{
	cout << "P " << directory.path << "/" << playlist.name << endl;
	return true;
}

int
main(int argc, char **argv)
{
	GError *error = nullptr;

	if (argc != 3) {
		cerr << "Usage: DumpDatabase CONFIG PLUGIN" << endl;
		return 1;
	}

	const char *const config_path = argv[1];
	const char *const plugin_name = argv[2];

	const DatabasePlugin *plugin = GetDatabasePluginByName(plugin_name);
	if (plugin == NULL) {
		cerr << "No such database plugin: " << plugin_name << endl;
		return EXIT_FAILURE;
	}

	/* initialize GLib */

	g_thread_init(nullptr);
	g_log_set_default_handler(my_log_func, nullptr);

	/* initialize MPD */

	config_global_init();

	if (!config_read_file(config_path, &error)) {
		cerr << error->message << endl;
		g_error_free(error);
		return EXIT_FAILURE;
	}

	tag_lib_init();

	/* do it */

	const struct config_param *path = config_get_param(CONF_DB_FILE);
	struct config_param *param = config_new_param("database", path->line);
	if (path != nullptr)
		config_add_block_param(param, "path", path->value, path->line);

	Database *db = plugin->create(param, &error);

	config_param_free(param);

	if (db == nullptr) {
		cerr << error->message << endl;
		g_error_free(error);
		return EXIT_FAILURE;
	}

	if (!db->Open(&error)) {
		delete db;
		cerr << error->message << endl;
		g_error_free(error);
		return EXIT_FAILURE;
	}

	const DatabaseSelection selection("", true);

	if (!db->Visit(selection, DumpDirectory, DumpSong, DumpPlaylist,
		       &error)) {
		db->Close();
		delete db;
		cerr << error->message << endl;
		g_error_free(error);
		return EXIT_FAILURE;
	}

	db->Close();
	delete db;

	/* deinitialize everything */

	config_global_finish();

	return EXIT_SUCCESS;
}
