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

#ifndef MPD_NEIGHBOR_ALL_HXX
#define MPD_NEIGHBOR_ALL_HXX

#include "thread/Mutex.hxx"

#include <forward_list>
#include <memory>
#include <string>

struct ConfigData;
class EventLoop;
class NeighborExplorer;
class NeighborListener;
struct NeighborInfo;

/**
 * A class that initializes and opens all configured neighbor plugins.
 */
class NeighborGlue {
	struct Explorer {
		const std::string name;
		std::unique_ptr<NeighborExplorer> explorer;

		template<typename N, typename E>
		Explorer(N &&_name, E &&_explorer) noexcept
			:name(std::forward<N>(_name)),
			 explorer(std::forward<E>(_explorer)) {}

		Explorer(const Explorer &) = delete;
	};

	Mutex mutex;

	std::forward_list<Explorer> explorers;

public:
	typedef std::forward_list<NeighborInfo> List;

	NeighborGlue() noexcept;
	NeighborGlue(const NeighborGlue &) = delete;
	~NeighborGlue() noexcept;

	bool IsEmpty() const noexcept {
		return explorers.empty();
	}

	/**
	 * Throws std::runtime_error on error.
	 */
	void Init(const ConfigData &config, EventLoop &loop,
		  NeighborListener &listener);

	void Open();
	void Close() noexcept;

	/**
	 * Get the combined list of all neighbors from all active
	 * plugins.
	 */
	[[gnu::pure]]
	List GetList() const noexcept;
};

#endif
