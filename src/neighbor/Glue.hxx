// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
