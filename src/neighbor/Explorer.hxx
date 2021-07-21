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

#ifndef MPD_NEIGHBOR_EXPLORER_HXX
#define MPD_NEIGHBOR_EXPLORER_HXX

#include <forward_list>

class NeighborListener;
struct NeighborInfo;

/**
 * An object that explores the neighborhood for music servers.
 *
 * As soon as this object is opened, it will start exploring, and
 * notify the #NeighborListener when it found or lost something.
 *
 * The implementation is supposed to be non-blocking.  This can be
 * implemented either using the #EventLoop instance that was passed to
 * the NeighborPlugin or by moving the blocking parts in a dedicated
 * thread.
 */
class NeighborExplorer {
protected:
	NeighborListener &listener;

	explicit NeighborExplorer(NeighborListener &_listener) noexcept
		:listener(_listener) {}

public:
	typedef std::forward_list<NeighborInfo> List;

	/**
	 * Free instance data.
         */
	virtual ~NeighborExplorer() noexcept = default;

	/**
         * Start exploring the neighborhood.
	 *
	 * Throws std::runtime_error on error.
	 */
	virtual void Open() = 0;

	/**
         * Stop exploring.
	 */
	virtual void Close() noexcept = 0;

	/**
	 * Obtain a list of currently known neighbors.
	 */
	virtual List GetList() const noexcept = 0;
};

#endif
