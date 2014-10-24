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
#include "db/Registry.hxx"
#include "db/DatabasePlugin.hxx"
#include "db/Interface.hxx"
#include "db/Selection.hxx"
#include "db/DatabaseListener.hxx"
#include "db/LightDirectory.hxx"
#include "db/LightSong.hxx"
#include "db/PlaylistVector.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigData.hxx"
#include "tag/TagConfig.hxx"
#include "fs/Path.hxx"
#include "event/Loop.hxx"
#include "util/Error.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <stdlib.h>

#ifdef HAVE_LIBUPNP
#include "input/InputStream.hxx"
size_t
InputStream::LockRead(void *, size_t, Error &)
{
	return 0;
}
#endif

class MyDatabaseListener final : public DatabaseListener {
public:
	virtual void OnDatabaseModified() override {
		cout << "DatabaseModified" << endl;
	}

	virtual void OnDatabaseSongRemoved(const LightSong &song) override {
		cout << "SongRemoved " << song.GetURI() << endl;
	}
};

static bool
DumpDirectory(const LightDirectory &directory, Error &)
{
	cout << "D " << directory.GetPath() << endl;
	return true;
}

static bool
DumpSong(const LightSong &song, Error &)
{
	cout << "S ";
	if (song.directory != nullptr)
		cout << song.directory << "/";
	cout << song.uri << endl;
	return true;
}

static bool
DumpPlaylist(const PlaylistInfo &playlist,
	     const LightDirectory &directory, Error &)
{
	cout << "P " << directory.GetPath()
	     << "/" << playlist.name.c_str() << endl;
	return true;
}

int
main(int argc, char **argv)
{
	if (argc != 3) {
		cerr << "Usage: DumpDatabase CONFIG PLUGIN" << endl;
		return 1;
	}

	const Path config_path = Path::FromFS(argv[1]);
	const char *const plugin_name = argv[2];

	const DatabasePlugin *plugin = GetDatabasePluginByName(plugin_name);
	if (plugin == NULL) {
		cerr << "No such database plugin: " << plugin_name << endl;
		return EXIT_FAILURE;
	}

	/* initialize GLib */

#ifdef HAVE_GLIB
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(nullptr);
#endif
#endif

	/* initialize MPD */

	config_global_init();

	Error error;
	if (!ReadConfigFile(config_path, error)) {
		cerr << error.GetMessage() << endl;
		return EXIT_FAILURE;
	}

	TagLoadConfig();

	EventLoop event_loop;
	MyDatabaseListener database_listener;

	/* do it */

	const struct config_param *path = config_get_param(CONF_DB_FILE);
	config_param param("database", path != nullptr ? path->line : -1);
	if (path != nullptr)
		param.AddBlockParam("path", path->value.c_str(), path->line);

	Database *db = plugin->create(event_loop, database_listener,
				      param, error);

	if (db == nullptr) {
		cerr << error.GetMessage() << endl;
		return EXIT_FAILURE;
	}

	if (!db->Open(error)) {
		delete db;
		cerr << error.GetMessage() << endl;
		return EXIT_FAILURE;
	}

	const DatabaseSelection selection("", true);

	if (!db->Visit(selection, DumpDirectory, DumpSong, DumpPlaylist,
		       error)) {
		db->Close();
		delete db;
		cerr << error.GetMessage() << endl;
		return EXIT_FAILURE;
	}

	db->Close();
	delete db;

	/* deinitialize everything */

	config_global_finish();

	return EXIT_SUCCESS;
}
