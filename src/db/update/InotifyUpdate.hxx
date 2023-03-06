// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INOTIFY_UPDATE_HXX
#define MPD_INOTIFY_UPDATE_HXX

#include "InotifyQueue.hxx"
#include "event/InotifyEvent.hxx"

#include <map>
#include <memory>

class Path;
class Storage;
struct WatchDirectory;

/**
 * Glue code between InotifySource and InotifyQueue.
 */
class InotifyUpdate final : InotifyHandler {
	InotifyEvent inotify_event;
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
	void AddToMap(WatchDirectory &directory) noexcept;
	void RemoveFromMap(WatchDirectory &directory) noexcept;
	void Disable(WatchDirectory &directory) noexcept;
	void Delete(WatchDirectory &directory) noexcept;

	void RecursiveWatchSubdirectories(WatchDirectory &parent,
					  Path path_fs,
					  unsigned depth) noexcept;

private:
	/* virtual methods from class InotifyHandler */
	void OnInotify(int wd, unsigned mask, const char *name) override;
	void OnInotifyError(std::exception_ptr error) noexcept override;
};

/**
 * Throws on error.
 */
std::unique_ptr<InotifyUpdate>
mpd_inotify_init(EventLoop &loop, Storage &storage, UpdateService &update,
		 unsigned max_depth);

#endif
