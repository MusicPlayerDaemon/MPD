// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "InotifyUpdate.hxx"
#include "InotifyDomain.hxx"
#include "ExcludeList.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "storage/StorageInterface.hxx"
#include "input/InputStream.hxx"
#include "input/Error.hxx"
#include "input/LocalOpen.hxx"
#include "input/WaitReady.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/DirectoryReader.hxx"
#include "fs/FileInfo.hxx"
#include "fs/Traits.hxx"
#include "thread/Mutex.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/IntrusiveList.hxx"
#include "Log.hxx"

#include <cassert>
#include <cstring>
#include <string>

#include <sys/inotify.h>
#include <dirent.h>
#include <errno.h>
#include <string.h> // for strerror()

static constexpr unsigned IN_MASK =
#ifdef IN_MASK_CREATE
	// since Linux 4.18
	IN_MASK_CREATE|
#endif
	IN_ONLYDIR|
	IN_ATTRIB|IN_CLOSE_WRITE|IN_CREATE|IN_DELETE|IN_DELETE_SELF
	|IN_MOVE|IN_MOVE_SELF;

class InotifyUpdate::Directory final : public InotifyWatch, public IntrusiveListHook<> {
	InotifyQueue &queue;

	Directory *parent;

	const AllocatedPath name;

	ExcludeList exclude_list;

	IntrusiveList<Directory> children;

	const unsigned remaining_depth;

public:
	template<typename N>
	Directory(InotifyManager &_manager, InotifyQueue &_queue,
		       N &&_name,
		       unsigned _remaining_depth)
		:InotifyWatch(_manager), queue(_queue),
		 parent(nullptr), name(std::forward<N>(_name)),
		 remaining_depth(_remaining_depth) {}

	template<typename N>
	Directory(Directory &_parent, N &&_name)
		:InotifyWatch(_parent.GetManager()), queue(_parent.queue),
		 parent(&_parent), name(std::forward<N>(_name)),
		 exclude_list(_parent.exclude_list),
		 remaining_depth(_parent.remaining_depth - 1) {}

	~Directory() noexcept {
		children.clear_and_dispose(DeleteDisposer{});
	}

	Directory(const Directory &) = delete;
	Directory &operator=(const Directory &) = delete;

	void LoadExcludeList(Path directory_path) noexcept;

	[[nodiscard]] [[gnu::pure]]
	AllocatedPath GetUriFS() const noexcept;

	void RecursiveWatchSubdirectories(Path path_fs) noexcept;

private:
	const Directory &GetRoot() const noexcept {
		const Directory *directory = this;
		while (directory->parent != nullptr)
			directory = directory->parent;
		return *directory;
	}

	void Delete() noexcept;

protected:
	void OnInotify(unsigned mask, const char *name) noexcept override;
};

void
InotifyUpdate::Directory::LoadExcludeList(Path directory_path) noexcept
try {
	Mutex mutex;
	auto is = OpenLocalInputStream(directory_path / Path::FromFS(".mpdignore"),
				       mutex);
	LockWaitReady(*is);
	exclude_list.Load(std::move(is));
} catch (...) {
	if (!IsFileNotFound(std::current_exception()))
		LogError(std::current_exception());
}

void
InotifyUpdate::Directory::Delete() noexcept
{
	if (parent == nullptr) {
		LogWarning(inotify_domain,
			   "music directory was removed - "
			   "cannot continue to watch it");
		return;
	}

	parent->children.erase_and_dispose(parent->children.iterator_to(*this),
					   DeleteDisposer{});
}

AllocatedPath
InotifyUpdate::Directory::GetUriFS() const noexcept
{
	if (parent == nullptr)
		return nullptr;

	const auto uri = parent->GetUriFS();
	if (uri.IsNull())
		return name;

	return uri / name;
}

/* we don't look at "." / ".." nor files with newlines in their name */
[[gnu::pure]]
static bool
SkipFilename(Path name) noexcept
{
	return PathTraitsFS::IsSpecialFilename(name.c_str()) ||
		name.HasNewline();
}

void
InotifyUpdate::Directory::RecursiveWatchSubdirectories(const Path path_fs) noexcept
try {
	assert(!path_fs.IsNull());

	if (remaining_depth == 0)
		return;

	DirectoryReader dir(path_fs);
	while (dir.ReadEntry()) {
		const Path name_fs = dir.GetEntry();
		if (SkipFilename(name_fs))
			continue;

		if (exclude_list.Check(name_fs))
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

		auto *child = new Directory(*this, name_fs);
		if (!child->TryAddWatch(child_path_fs.c_str(), IN_MASK)) {
			delete child;

			const int e = errno;
			if (e == EEXIST) {
				/* already registered (see IN_MASK_CREATE) */
				continue;
			}

			FmtError(inotify_domain,
				 "Failed to register {}: {}",
				 child_path_fs, strerror(e));
			continue;
		}

		children.push_back(*child);
		child->LoadExcludeList(child_path_fs);
		child->RecursiveWatchSubdirectories(child_path_fs);
	}
} catch (...) {
	LogError(std::current_exception());
}

inline
InotifyUpdate::InotifyUpdate(EventLoop &loop, UpdateService &update,
			     unsigned _max_depth)
	:inotify_manager(loop),
	 queue(loop, update),
	 max_depth(_max_depth)
{
}

InotifyUpdate::~InotifyUpdate() noexcept = default;

inline void
InotifyUpdate::Start(Path path)
{
	root = std::make_unique<Directory>(inotify_manager, queue, path, max_depth);
	root->AddWatch(path.c_str(), IN_MASK);
	root->LoadExcludeList(path);
	root->RecursiveWatchSubdirectories(path);
}

void
InotifyUpdate::Directory::OnInotify(unsigned mask, const char *) noexcept
{
	const auto uri_fs = GetUriFS();

	if ((mask & (IN_DELETE_SELF|IN_MOVE_SELF)) != 0) {
		Delete();
		return;
	}

	if ((mask & (IN_ATTRIB|IN_CREATE|IN_MOVE)) != 0 &&
	    (mask & IN_ISDIR) != 0) {
		/* a sub directory was changed: register those in
		   inotify */
		const auto &root_path = GetRoot().name;

		const auto path_fs = uri_fs.IsNull()
			? root_path
			: (root_path / uri_fs);

		RecursiveWatchSubdirectories(path_fs);
	}

	if ((mask & (IN_CLOSE_WRITE|IN_MOVE|IN_DELETE)) != 0 ||
	    /* regular file or symlink was created; this check is only
	       interesting for symlinks because regular files have
	       usable content only after IN_CLOSE_WRITE */
	    (mask & (IN_CREATE|IN_ISDIR)) == IN_CREATE ||
	    /* at the maximum depth, we watch out for newly created
	       directories */
	    (remaining_depth == 0 &&
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
