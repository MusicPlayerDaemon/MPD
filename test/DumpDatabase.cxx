/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "song/LightSong.hxx"
#include "db/PlaylistVector.hxx"
#include "ConfigGlue.hxx"
#include "tag/Config.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "event/Thread.hxx"
#include "util/ScopeExit.hxx"
#include "util/PrintException.hxx"

#include <stdexcept>
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <stdlib.h>

class GlobalInit {
	EventThread io_thread;

public:
	GlobalInit() {
		io_thread.Start();
	}

	~GlobalInit() = default;

	EventLoop &GetEventLoop() {
		return io_thread.GetEventLoop();
	}
};

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
	void OnDatabaseModified() noexcept override {
		cout << "DatabaseModified" << endl;
	}

	void OnDatabaseSongRemoved(const char *uri) noexcept override {
		cout << "SongRemoved " << uri << endl;
	}
};

static void
DumpDirectory(const LightDirectory &directory)
{
	cout << "D " << directory.GetPath() << endl;
}

static void
DumpSong(const LightSong &song)
{
	cout << "S ";
	if (song.directory != nullptr)
		cout << song.directory << "/";
	cout << song.uri << endl;
}

static void
DumpPlaylist(const PlaylistInfo &playlist, const LightDirectory &directory)
{
	cout << "P " << directory.GetPath()
	     << "/" << playlist.name.c_str() << endl;
}

int
main(int argc, char **argv)
try {
	if (argc != 3) {
		cerr << "Usage: DumpDatabase CONFIG PLUGIN" << endl;
		return 1;
	}

	const FromNarrowPath config_path = argv[1];
	const char *const plugin_name = argv[2];

	const DatabasePlugin *plugin = GetDatabasePluginByName(plugin_name);
	if (plugin == nullptr) {
		cerr << "No such database plugin: " << plugin_name << endl;
		return EXIT_FAILURE;
	}

	/* initialize MPD */

	GlobalInit init;

	const auto config = AutoLoadConfigFile(config_path);

	TagLoadConfig(config);

	MyDatabaseListener database_listener;

	/* do it */

	const auto *path = config.GetParam(ConfigOption::DB_FILE);
	ConfigBlock block(path != nullptr ? path->line : -1);
	if (path != nullptr)
		block.AddBlockParam("path", path->value, path->line);

	auto db = plugin->create(init.GetEventLoop(),
				 init.GetEventLoop(),
				 database_listener, block);

	db->Open();

	AtScopeExit(&db) { db->Close(); };

	const DatabaseSelection selection("", true);

	db->Visit(selection, DumpDirectory, DumpSong, DumpPlaylist);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
