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

#ifndef MPD_INOTIFY_UPDATE_HXX
#define MPD_INOTIFY_UPDATE_HXX

#include "InotifySource.hxx"
#include "InotifyQueue.hxx"

#include <map>
#include <memory>

class Path;
class Storage;
struct WatchDirectory;

/**
 * Glue code between InotifySource and InotifyQueue.
 */
class InotifyUpdate {
	InotifySource source;
	InotifyQueue queue;

	const unsigned max_depth;

	std::unique_ptr<WatchDirectory> root;
	std::map<int, WatchDirectory *> directories;

public:
	InotifyUpdate(EventLoop &loop, UpdateService &update,
		      unsigned _max_depth);
	~InotifyUpdate() noexcept;

	void Start(Path path);

private:
	void InotifyCallback(int wd, unsigned mask, const char *name) noexcept;

	static void InotifyCallback(int wd, unsigned mask,
				    const char *name, void *ctx) noexcept {
		auto &iu = *(InotifyUpdate *)ctx;
		iu.InotifyCallback(wd, mask, name);
	}

	void AddToMap(WatchDirectory &directory) noexcept;
	void RemoveFromMap(WatchDirectory &directory) noexcept;
	void Disable(WatchDirectory &directory) noexcept;
	void Delete(WatchDirectory &directory) noexcept;

	void RecursiveWatchSubdirectories(WatchDirectory &parent,
					  Path path_fs,
					  unsigned depth) noexcept;
};

/**
 * Throws on error.
 */
std::unique_ptr<InotifyUpdate>
mpd_inotify_init(EventLoop &loop, Storage &storage, UpdateService &update,
		 unsigned max_depth);

#endif
