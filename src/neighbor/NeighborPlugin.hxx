// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NEIGHBOR_PLUGIN_HXX
#define MPD_NEIGHBOR_PLUGIN_HXX

#include <memory>

struct ConfigBlock;
class EventLoop;
class NeighborListener;
class NeighborExplorer;

struct NeighborPlugin {
	const char *name;

	/**
	 * Allocates and configures a #NeighborExplorer instance.
	 */
	std::unique_ptr<NeighborExplorer> (*create)(EventLoop &loop,
						    NeighborListener &listener,
						    const ConfigBlock &block);
};

#endif
