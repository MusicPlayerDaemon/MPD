// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NEIGHBOR_REGISTRY_HXX
#define MPD_NEIGHBOR_REGISTRY_HXX

struct NeighborPlugin;

/**
 * nullptr terminated list of all neighbor plugins which were enabled at
 * compile time.
 */
extern const NeighborPlugin *const neighbor_plugins[];

[[gnu::pure]]
const NeighborPlugin *
GetNeighborPluginByName(const char *name) noexcept;

#endif
