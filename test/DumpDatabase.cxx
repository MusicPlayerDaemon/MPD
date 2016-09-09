/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "config/Param.hxx"
#include "config/Block.hxx"
#include "tag/TagConfig.hxx"
#include "fs/Path.hxx"
#include "event/Loop.hxx"
#include "Log.hxx"
#include "util/Error.hxx"
#include "util/ScopeExit.hxx"

#include <stdexcept>
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <stdlib.h>

#ifdef ENABLE_UPNP
#include "input/InputStream.hxx"
size_t
InputStream::LockRead(void *, size_t)
{
	return 0;
}
#endif

class MyDatabaseListener final : public DatabaseListener {
public:
	virtual void OnDatabaseModified() override {
		cout << "DatabaseModified" << endl;
	}

	virtual void OnDatabaseSongRemoved(const char *uri) override {
		cout << "SongRemoved " << uri << endl;
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
try {
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

	/* initialize MPD */

	config_global_init();
	AtScopeExit() { config_global_finish(); };

	Error error;
	ReadConfigFile(config_path);

	TagLoadConfig();

	EventLoop event_loop;
	MyDatabaseListener database_listener;

	/* do it */

	const auto *path = config_get_param(ConfigOption::DB_FILE);
	ConfigBlock block(path != nullptr ? path->line : -1);
	if (path != nullptr)
		block.AddBlockParam("path", path->value.c_str(), path->line);

	Database *db = plugin->create(event_loop, database_listener,
				      block, error);

	if (db == nullptr) {
		cerr << error.GetMessage() << endl;
		return EXIT_FAILURE;
	}

	AtScopeExit(db) { delete db; };

	db->Open();

	AtScopeExit(db) { db->Close(); };

	const DatabaseSelection selection("", true);

	if (!db->Visit(selection, DumpDirectory, DumpSong, DumpPlaylist,
		       error)) {
		cerr << error.GetMessage() << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
 } catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
 }
