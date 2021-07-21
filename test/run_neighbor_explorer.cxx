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

#include "ConfigGlue.hxx"
#include "neighbor/Listener.hxx"
#include "neighbor/Info.hxx"
#include "neighbor/Glue.hxx"
#include "fs/Path.hxx"
#include "event/Loop.hxx"
#include "ShutdownHandler.hxx"
#include "util/PrintException.hxx"

#include <stdio.h>
#include <stdlib.h>

class MyNeighborListener final : public NeighborListener {
 public:
	/* virtual methods from class NeighborListener */
	void FoundNeighbor(const NeighborInfo &info) noexcept override {
		printf("found '%s' (%s)\n",
		       info.display_name.c_str(), info.uri.c_str());
	}

	void LostNeighbor(const NeighborInfo &info) noexcept override {
		printf("lost '%s' (%s)\n",
		       info.display_name.c_str(), info.uri.c_str());
	}
};

int
main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: run_neighbor_explorer CONFIG\n");
		return EXIT_FAILURE;
	}

	const Path config_path = Path::FromFS(argv[1]);

	/* initialize the core */

	EventLoop loop;
	const ShutdownHandler shutdown_handler(loop);

	/* read configuration file (mpd.conf) */

	const auto config = AutoLoadConfigFile(config_path);

	/* initialize neighbor plugins */

	MyNeighborListener listener;
	NeighborGlue neighbor;
	neighbor.Init(config, loop, listener);
	neighbor.Open();

	/* dump initial list */

	for (const auto &info : neighbor.GetList())
		printf("have '%s' (%s)\n",
		       info.display_name.c_str(), info.uri.c_str());

	/* run */

	loop.Run();
	neighbor.Close();
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
