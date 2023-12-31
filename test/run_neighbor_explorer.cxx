// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ConfigGlue.hxx"
#include "neighbor/Listener.hxx"
#include "neighbor/Info.hxx"
#include "neighbor/Glue.hxx"
#include "fs/NarrowPath.hxx"
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

	const FromNarrowPath config_path = argv[1];

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
