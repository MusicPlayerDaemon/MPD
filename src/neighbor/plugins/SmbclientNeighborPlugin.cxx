// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SmbclientNeighborPlugin.hxx"
#include "lib/smbclient/Init.hxx"
#include "lib/smbclient/Context.hxx"
#include "lib/smbclient/Domain.hxx"
#include "neighbor/NeighborPlugin.hxx"
#include "neighbor/Explorer.hxx"
#include "neighbor/Listener.hxx"
#include "neighbor/Info.hxx"
#include "protocol/Verify.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "thread/ScopeUnlock.hxx"
#include "thread/Thread.hxx"
#include "thread/Name.hxx"
#include "Log.hxx"

#include <libsmbclient.h>

#include <cerrno>
#include <cstring>
#include <utility>

using std::string_view_literals::operator""sv;

class SmbclientNeighborExplorer final : public NeighborExplorer {
	SmbclientContext ctx = SmbclientContext::New();

	Thread thread;

	mutable Mutex mutex;
	Cond cond;

	List list;

	bool quit;

public:
	explicit SmbclientNeighborExplorer(NeighborListener &_listener)
		:NeighborExplorer(_listener),
		 thread(BIND_THIS_METHOD(ThreadFunc)) {}

	/* virtual methods from class NeighborExplorer */
	void Open() override;
	void Close() noexcept override;
	List GetList() const noexcept override;

private:
	/**
	 * Caller must lock the mutex.
	 */
	void Run() noexcept;

	void ThreadFunc() noexcept;
};

void
SmbclientNeighborExplorer::Open()
{
	quit = false;
	thread.Start();
}

void
SmbclientNeighborExplorer::Close() noexcept
{
	{
		const std::lock_guard lock{mutex};
		quit = true;
		cond.notify_one();
	}

	thread.Join();
}

NeighborExplorer::List
SmbclientNeighborExplorer::GetList() const noexcept
{
	const std::lock_guard protect{mutex};
	return list;
}

[[gnu::pure]]
static bool
IsValidSmbServerName(std::string_view name) noexcept
{
	/* this is more relaxed than the names actually allowed in the
	   protocol, but good enough to protect MPD from crafted
	   names */
	return VerifyFilenameUTF8(name);
}

static void
ReadServer(NeighborExplorer::List &list, const smbc_dirent &e) noexcept
{
	const std::string_view name{e.name, e.namelen};
	const std::string_view comment{e.comment, e.commentlen};

	if (!IsValidSmbServerName(name))
		return;

	list.emplace_front(fmt::format("smb://{}"sv, name),
			   fmt::format("{} ({})"sv, name, comment));
}

static void
ReadServers(SmbclientContext &ctx, const char *uri,
	    NeighborExplorer::List &list) noexcept;

static void
ReadWorkgroup(SmbclientContext &ctx, std::string_view name,
	      NeighborExplorer::List &list) noexcept
{
	if (!IsValidSmbServerName(name))
		return;

	ReadServers(ctx, fmt::format("smb://{}"sv, name).c_str(), list);
}

static void
ReadEntry(SmbclientContext &ctx, const smbc_dirent &e,
	  NeighborExplorer::List &list) noexcept
{
	switch (e.smbc_type) {
	case SMBC_WORKGROUP:
		ReadWorkgroup(ctx, {e.name, e.namelen}, list);
		break;

	case SMBC_SERVER:
		ReadServer(list, e);
		break;
	}
}

static void
ReadServers(SmbclientContext &ctx, SMBCFILE *handle,
	    NeighborExplorer::List &list) noexcept
{
	while (auto e = ctx.ReadDirectory(handle))
		ReadEntry(ctx, *e, list);
}

static void
ReadServers(SmbclientContext &ctx, const char *uri,
	    NeighborExplorer::List &list) noexcept
{
	SMBCFILE *handle = ctx.OpenDirectory(uri);
	if (handle != nullptr) {
		ReadServers(ctx, handle, list);
		ctx.CloseDirectory(handle);
	} else
		FmtError(smbclient_domain, "smbc_opendir({:?}) failed: {}",
			 uri, strerror(errno));
}

[[gnu::pure]]
static NeighborExplorer::List
DetectServers(SmbclientContext &ctx) noexcept
{
	NeighborExplorer::List list;
	ReadServers(ctx, "smb://", list);
	return list;
}

[[gnu::pure]]
static NeighborExplorer::List::iterator
FindBeforeServerByURI(NeighborExplorer::List::iterator prev,
		      NeighborExplorer::List::iterator end,
		      const std::string &uri) noexcept
{
	for (auto i = std::next(prev); i != end; prev = i, i = std::next(prev))
		if (i->uri == uri)
			return prev;

	return end;
}

inline void
SmbclientNeighborExplorer::Run() noexcept
{
	List found, lost;

	{
		const ScopeUnlock unlock(mutex);
		found = DetectServers(ctx);
	}

	const auto found_before_begin = found.before_begin();
	const auto found_end = found.end();

	for (auto prev = list.before_begin(), i = std::next(prev), end = list.end();
	     i != end; i = std::next(prev)) {
		auto f = FindBeforeServerByURI(found_before_begin, found_end,
					       i->uri);
		if (f != found_end) {
			/* still visible: remove from "found" so we
			   don't believe it's a new one */
			*i = std::move(*std::next(f));
			found.erase_after(f);
			prev = i;
		} else {
			/* can't see it anymore: move to "lost" */
			lost.splice_after(lost.before_begin(), list, prev);
		}
	}

	for (auto prev = found_before_begin, i = std::next(prev);
	     i != found_end; prev = i, i = std::next(prev))
		list.push_front(*i);

	const ScopeUnlock unlock(mutex);

	for (auto &i : lost)
		listener.LostNeighbor(i);

	for (auto &i : found)
		listener.FoundNeighbor(i);
}

inline void
SmbclientNeighborExplorer::ThreadFunc() noexcept
{
	SetThreadName("smbclient");

	std::unique_lock lock{mutex};

	while (!quit) {
		Run();

		if (quit)
			break;

		// TODO: sleep for how long?
		cond.wait_for(lock, std::chrono::seconds(10));
	}
}

static std::unique_ptr<NeighborExplorer>
smbclient_neighbor_create([[maybe_unused]] EventLoop &loop,
			  NeighborListener &listener,
			  [[maybe_unused]] const ConfigBlock &block)
{
	SmbclientInit();

	return std::make_unique<SmbclientNeighborExplorer>(listener);
}

const NeighborPlugin smbclient_neighbor_plugin = {
	"smbclient",
	smbclient_neighbor_create,
};
