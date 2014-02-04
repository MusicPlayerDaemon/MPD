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
#include "config/ConfigGlobal.hxx"
#include "neighbor/Listener.hxx"
#include "neighbor/Info.hxx"
#include "neighbor/Glue.hxx"
#include "fs/Path.hxx"
#include "event/Loop.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <stdio.h>
#include <stdlib.h>

class MyNeighborListener final : public NeighborListener {
 public:
	/* virtual methods from class NeighborListener */
	virtual void FoundNeighbor(const NeighborInfo &info) override {
		printf("found '%s' (%s)\n",
		       info.display_name.c_str(), info.uri.c_str());
	}

	virtual void LostNeighbor(const NeighborInfo &info) override {
		printf("lost '%s' (%s)\n",
		       info.display_name.c_str(), info.uri.c_str());
	}
};

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: run_neighbor_explorer CONFIG\n");
		return EXIT_FAILURE;
	}

	const Path config_path = Path::FromFS(argv[1]);

	/* read configuration file (mpd.conf) */

	Error error;

	config_global_init();
	if (!ReadConfigFile(config_path, error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	/* initialize the core */

	EventLoop loop;

	/* initialize neighbor plugins */

	MyNeighborListener listener;
	NeighborGlue neighbor;
	if (!neighbor.Init(loop, listener, error) || !neighbor.Open(error)) {
		LogError(error);
		return EXIT_FAILURE;
	}

	/* run */

	loop.Run();
	neighbor.Close();
	return EXIT_SUCCESS;
}
