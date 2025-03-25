// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "InotifyQueue.hxx"
#include "event/InotifyManager.hxx"

#include <memory>

class Path;
class Storage;

/**
 * Glue code between InotifySource and InotifyQueue.
 */
class InotifyUpdate final {
	InotifyManager inotify_manager;
	InotifyQueue queue;

	const unsigned max_depth;

	class Directory;
	std::unique_ptr<Directory> root;

public:
	InotifyUpdate(EventLoop &loop, UpdateService &update,
		      unsigned _max_depth);
	~InotifyUpdate() noexcept;

	void Start(Path path);
};

/**
 * Throws on error.
 */
std::unique_ptr<InotifyUpdate>
mpd_inotify_init(EventLoop &loop, Storage &storage, UpdateService &update,
		 unsigned max_depth);
