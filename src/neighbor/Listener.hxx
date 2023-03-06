// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NEIGHBOR_LISTENER_HXX
#define MPD_NEIGHBOR_LISTENER_HXX

struct NeighborInfo;
class NeighborExplorer;

/**
 * An interface that listens on events from neighbor plugins.  The
 * methods must be thread-safe and non-blocking.
 */
class NeighborListener {
public:
	virtual void FoundNeighbor(const NeighborInfo &info) noexcept = 0;
	virtual void LostNeighbor(const NeighborInfo &info) noexcept = 0;
};

#endif
