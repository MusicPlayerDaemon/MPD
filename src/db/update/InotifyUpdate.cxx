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

#include "InotifyUpdate.hxx"
#include "InotifyDomain.hxx"
#include "ExcludeList.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "storage/StorageInterface.hxx"
#include "input/InputStream.hxx"
#include "input/Error.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/DirectoryReader.hxx"
#include "fs/FileInfo.hxx"
#include "fs/Traits.hxx"
#include "thread/Mutex.hxx"
#include "util/Compiler.h"
#include "Log.hxx"

#include <cassert>
#include <cstring>
#include <forward_list>
#include <string>

#include <sys/inotify.h>
#include <dirent.h>

static constexpr unsigned IN_MASK =
#ifdef IN_ONLYDIR
	IN_ONLYDIR|
#endif
	IN_ATTRIB|IN_CLOSE_WRITE|IN_CREATE|IN_DELETE|IN_DELETE_SELF
	|IN_MOVE|IN_MOVE_SELF;

struct WatchDirectory {
	WatchDirectory *parent;

	AllocatedPath name;

	int descriptor;

	ExcludeList exclude_list;

	std::forward_list<WatchDirectory> children;

	template<typename N>
	WatchDirectory(N &&_name,
		       int _descriptor)
		:parent(nullptr), name(std::forward<N>(_name)),
		 descriptor(_descriptor) {}

	template<typename N>
	WatchDirectory(WatchDirectory &_parent, N &&_name,
		       int _descriptor)
		:parent(&_parent), name(std::forward<N>(_name)),
		 descriptor(_descriptor),
		 exclude_list(_parent.exclude_list) {}

	WatchDirectory(const WatchDirectory &) = delete;
	WatchDirectory &operator=(const WatchDirectory &) = delete;

	void LoadExcludeList(Path directory_path) noexcept;

	[[nodiscard]] gcc_pure
	unsigned GetDepth() const noexcept;

	[[nodiscard]] gcc_pure
	AllocatedPath GetUriFS() const noexcept;
};

void
WatchDirectory::LoadExcludeList(Path directory_path) noexcept
try {
	Mutex mutex;
	auto is = InputStream::OpenReady((directory_path / Path::FromFS(".mpdignore")).c_str(),
					 mutex);
	exclude_list.Load(std::move(is));
} catch (...) {
	if (!IsFileNotFound(std::current_exception()))
		LogError(std::current_exception());
}

void
InotifyUpdate::AddToMap(WatchDirectory &directory) noexcept
{
	directories.emplace(directory.descriptor, &directory);
}

void
InotifyUpdate::RemoveFromMap(WatchDirectory &directory) noexcept
{
	auto i = directories.find(directory.descriptor);
	assert(i != directories.end());
	directories.erase(i);
}

void
InotifyUpdate::Disable(WatchDirectory &directory) noexcept
{
	RemoveFromMap(directory);

	for (WatchDirectory &child : directory.children)
		Disable(child);

	source.Remove(directory.descriptor);
}

void
InotifyUpdate::Delete(WatchDirectory &directory) noexcept
{
	if (directory.parent == nullptr) {
		LogWarning(inotify_domain,
			   "music directory was removed - "
			   "cannot continue to watch it");
		return;
	}

	Disable(directory);

	/* remove it from the parent, which effectively deletes it */
	directory.parent->children.remove_if([&directory](const WatchDirectory &child){
		return &child == &directory;
	});
}

AllocatedPath
WatchDirectory::GetUriFS() const noexcept
{
	if (parent == nullptr)
		return nullptr;

	const auto uri = parent->GetUriFS();
	if (uri.IsNull())
		return name;

	return uri / name;
}

/* we don't look at "." / ".." nor files with newlines in their name */
gcc_pure
static bool
SkipFilename(Path name) noexcept
{
	return PathTraitsFS::IsSpecialFilename(name.c_str()) ||
		name.HasNewline();
}

void
InotifyUpdate::RecursiveWatchSubdirectories(WatchDirectory &parent,
					    const Path path_fs,
					    unsigned depth) noexcept
try {
	assert(depth <= max_depth);
	assert(!path_fs.IsNull());

	++depth;

	if (depth > max_depth)
		return;

	DirectoryReader dir(path_fs);
	while (dir.ReadEntry()) {
		int ret;

		const Path name_fs = dir.GetEntry();
		if (SkipFilename(name_fs))
			continue;

		if (parent.exclude_list.Check(name_fs))
			continue;

		const auto child_path_fs = path_fs / name_fs;

		FileInfo fi;
		try {
			fi = FileInfo(child_path_fs);
		} catch (...) {
			LogError(std::current_exception());
			continue;
		}

		if (!fi.IsDirectory())
			continue;

		try {
			ret = source.Add(child_path_fs.c_str(), IN_MASK);
		} catch (...) {
			FmtError(inotify_domain,
				 "Failed to register {}: {}",
				 child_path_fs, std::current_exception());
			continue;
		}

		if (directories.find(ret) != directories.end())
			/* already being watched */
			continue;

		parent.children.emplace_front(parent,
					      name_fs,
					      ret);
		auto *child = &parent.children.front();
		child->LoadExcludeList(child_path_fs);

		AddToMap(*child);

		RecursiveWatchSubdirectories(*child, child_path_fs, depth);
	}
} catch (...) {
	LogError(std::current_exception());
}

gcc_pure
unsigned
WatchDirectory::GetDepth() const noexcept
{
	const WatchDirectory *d = this;
	unsigned depth = 0;
	while ((d = d->parent) != nullptr)
		++depth;

	return depth;
}

inline
InotifyUpdate::InotifyUpdate(EventLoop &loop, UpdateService &update,
			     unsigned _max_depth)
	:source(loop, InotifyCallback, this),
	 queue(loop, update),
	 max_depth(_max_depth)
{
}

InotifyUpdate::~InotifyUpdate() noexcept = default;

inline void
InotifyUpdate::Start(Path path)
{
	int descriptor = source.Add(path.c_str(), IN_MASK);

	root = std::make_unique<WatchDirectory>(path, descriptor);
	root->LoadExcludeList(path);

	AddToMap(*root);

	RecursiveWatchSubdirectories(*root, path, 0);
}

void
InotifyUpdate::InotifyCallback(int wd, unsigned mask,
			       [[maybe_unused]] const char *name) noexcept
{
	auto i = directories.find(wd);
	if (i == directories.end())
		return;

	auto &directory = *i->second;

	const auto uri_fs = directory.GetUriFS();

	if ((mask & (IN_DELETE_SELF|IN_MOVE_SELF)) != 0) {
		Delete(directory);
		return;
	}

	if ((mask & (IN_ATTRIB|IN_CREATE|IN_MOVE)) != 0 &&
	    (mask & IN_ISDIR) != 0) {
		/* a sub directory was changed: register those in
		   inotify */
		const auto root_path = root->name;

		const auto path_fs = uri_fs.IsNull()
			? root_path
			: (root_path / uri_fs);

		RecursiveWatchSubdirectories(directory, path_fs,
					     directory.GetDepth());
	}

	if ((mask & (IN_CLOSE_WRITE|IN_MOVE|IN_DELETE)) != 0 ||
	    /* at the maximum depth, we watch out for newly created
	       directories */
	    (directory.GetDepth() == max_depth &&
	     (mask & (IN_CREATE|IN_ISDIR)) == (IN_CREATE|IN_ISDIR))) {
		/* a file was changed, or a directory was
		   moved/deleted: queue a database update */

		if (!uri_fs.IsNull()) {
			const std::string uri_utf8 = uri_fs.ToUTF8();
			if (!uri_utf8.empty())
				queue.Enqueue(uri_utf8.c_str());
		}
		else
			queue.Enqueue("");
	}
}

std::unique_ptr<InotifyUpdate>
mpd_inotify_init(EventLoop &loop, Storage &storage, UpdateService &update,
		 unsigned max_depth)
{
	LogDebug(inotify_domain, "initializing inotify");

	const auto path = storage.MapFS("");
	if (path.IsNull()) {
		LogDebug(inotify_domain, "no music directory configured");
		return {};
	}

	auto iu = std::make_unique<InotifyUpdate>(loop, update, max_depth);
	iu->Start(path);

	LogDebug(inotify_domain, "watching music directory");

	return iu;
}
