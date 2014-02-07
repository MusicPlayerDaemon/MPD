/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h" /* must be first for large file support */
#include "InotifyUpdate.hxx"
#include "InotifySource.hxx"
#include "InotifyQueue.hxx"
#include "InotifyDomain.hxx"
#include "storage/StorageInterface.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <string>
#include <map>
#include <forward_list>

#include <assert.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <string.h>
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

	std::forward_list<WatchDirectory> children;

	template<typename N>
	WatchDirectory(WatchDirectory *_parent, N &&_name,
		       int _descriptor)
		:parent(_parent), name(std::forward<N>(_name)),
		 descriptor(_descriptor) {}

	WatchDirectory(const WatchDirectory &) = delete;
	WatchDirectory &operator=(const WatchDirectory &) = delete;

	gcc_pure
	unsigned GetDepth() const;

	gcc_pure
	AllocatedPath GetUriFS() const;
};

static InotifySource *inotify_source;
static InotifyQueue *inotify_queue;

static unsigned inotify_max_depth;
static WatchDirectory *inotify_root;
static std::map<int, WatchDirectory *> inotify_directories;

static void
tree_add_watch_directory(WatchDirectory *directory)
{
	inotify_directories.insert(std::make_pair(directory->descriptor,
						  directory));
}

static void
tree_remove_watch_directory(WatchDirectory *directory)
{
	auto i = inotify_directories.find(directory->descriptor);
	assert(i != inotify_directories.end());
	inotify_directories.erase(i);
}

static WatchDirectory *
tree_find_watch_directory(int wd)
{
	auto i = inotify_directories.find(wd);
	if (i == inotify_directories.end())
		return nullptr;

	return i->second;
}

static void
disable_watch_directory(WatchDirectory &directory)
{
	tree_remove_watch_directory(&directory);

	for (WatchDirectory &child : directory.children)
		disable_watch_directory(child);

	inotify_source->Remove(directory.descriptor);
}

static void
remove_watch_directory(WatchDirectory *directory)
{
	assert(directory != nullptr);

	if (directory->parent == nullptr) {
		LogWarning(inotify_domain,
			   "music directory was removed - "
			   "cannot continue to watch it");
		return;
	}

	disable_watch_directory(*directory);

	/* remove it from the parent, which effectively deletes it */
	directory->parent->children.remove_if([directory](const WatchDirectory &child){
			return &child == directory;
		});
}

AllocatedPath
WatchDirectory::GetUriFS() const
{
	if (parent == nullptr)
		return AllocatedPath::Null();

	const auto uri = parent->GetUriFS();
	if (uri.IsNull())
		return name;

	return AllocatedPath::Build(uri, name);
}

/* we don't look at "." / ".." nor files with newlines in their name */
static bool skip_path(const char *path)
{
	return (path[0] == '.' && path[1] == 0) ||
		(path[0] == '.' && path[1] == '.' && path[2] == 0) ||
		strchr(path, '\n') != nullptr;
}

static void
recursive_watch_subdirectories(WatchDirectory *directory,
			       const AllocatedPath &path_fs, unsigned depth)
{
	Error error;
	DIR *dir;
	struct dirent *ent;

	assert(directory != nullptr);
	assert(depth <= inotify_max_depth);
	assert(!path_fs.IsNull());

	++depth;

	if (depth > inotify_max_depth)
		return;

	dir = opendir(path_fs.c_str());
	if (dir == nullptr) {
		FormatErrno(inotify_domain,
			    "Failed to open directory %s", path_fs.c_str());
		return;
	}

	while ((ent = readdir(dir))) {
		struct stat st;
		int ret;

		if (skip_path(ent->d_name))
			continue;

		const auto child_path_fs =
			AllocatedPath::Build(path_fs, ent->d_name);
		ret = StatFile(child_path_fs, st);
		if (ret < 0) {
			FormatErrno(inotify_domain,
				    "Failed to stat %s",
				    child_path_fs.c_str());
			continue;
		}

		if (!S_ISDIR(st.st_mode))
			continue;

		ret = inotify_source->Add(child_path_fs.c_str(), IN_MASK,
					  error);
		if (ret < 0) {
			FormatError(error,
				    "Failed to register %s",
				    child_path_fs.c_str());
			error.Clear();
			continue;
		}

		WatchDirectory *child = tree_find_watch_directory(ret);
		if (child != nullptr)
			/* already being watched */
			continue;

		directory->children.emplace_front(directory,
						  AllocatedPath::FromFS(ent->d_name),
						  ret);
		child = &directory->children.front();

		tree_add_watch_directory(child);

		recursive_watch_subdirectories(child, child_path_fs, depth);
	}

	closedir(dir);
}

gcc_pure
unsigned
WatchDirectory::GetDepth() const
{
	const WatchDirectory *d = this;
	unsigned depth = 0;
	while ((d = d->parent) != nullptr)
		++depth;

	return depth;
}

static void
mpd_inotify_callback(int wd, unsigned mask,
		     gcc_unused const char *name, gcc_unused void *ctx)
{
	WatchDirectory *directory;

	/*FormatDebug(inotify_domain, "wd=%d mask=0x%x name='%s'", wd, mask, name);*/

	directory = tree_find_watch_directory(wd);
	if (directory == nullptr)
		return;

	const auto uri_fs = directory->GetUriFS();

	if ((mask & (IN_DELETE_SELF|IN_MOVE_SELF)) != 0) {
		remove_watch_directory(directory);
		return;
	}

	if ((mask & (IN_ATTRIB|IN_CREATE|IN_MOVE)) != 0 &&
	    (mask & IN_ISDIR) != 0) {
		/* a sub directory was changed: register those in
		   inotify */
		const auto &root = inotify_root->name;

		const auto path_fs = uri_fs.IsNull()
			? root
			: AllocatedPath::Build(root, uri_fs.c_str());

		recursive_watch_subdirectories(directory, path_fs,
					       directory->GetDepth());
	}

	if ((mask & (IN_CLOSE_WRITE|IN_MOVE|IN_DELETE)) != 0 ||
	    /* at the maximum depth, we watch out for newly created
	       directories */
	    (directory->GetDepth() == inotify_max_depth &&
	     (mask & (IN_CREATE|IN_ISDIR)) == (IN_CREATE|IN_ISDIR))) {
		/* a file was changed, or a directory was
		   moved/deleted: queue a database update */

		if (!uri_fs.IsNull()) {
			const std::string uri_utf8 = uri_fs.ToUTF8();
			if (!uri_utf8.empty())
				inotify_queue->Enqueue(uri_utf8.c_str());
		}
		else
			inotify_queue->Enqueue("");
	}
}

void
mpd_inotify_init(EventLoop &loop, Storage &storage, UpdateService &update,
		 unsigned max_depth)
{
	LogDebug(inotify_domain, "initializing inotify");

	const auto path = storage.MapFS("");
	if (path.IsNull()) {
		LogDebug(inotify_domain, "no music directory configured");
		return;
	}

	Error error;
	inotify_source = InotifySource::Create(loop,
					       mpd_inotify_callback, nullptr,
					       error);
	if (inotify_source == nullptr) {
		LogError(error);
		return;
	}

	inotify_max_depth = max_depth;

	int descriptor = inotify_source->Add(path.c_str(), IN_MASK, error);
	if (descriptor < 0) {
		LogError(error);
		delete inotify_source;
		inotify_source = nullptr;
		return;
	}

	inotify_root = new WatchDirectory(nullptr, path, descriptor);

	tree_add_watch_directory(inotify_root);

	recursive_watch_subdirectories(inotify_root, path, 0);

	inotify_queue = new InotifyQueue(loop, update);

	LogDebug(inotify_domain, "watching music directory");
}

void
mpd_inotify_finish(void)
{
	if (inotify_source == nullptr)
		return;

	delete inotify_queue;
	delete inotify_source;
	delete inotify_root;
	inotify_directories.clear();
}
