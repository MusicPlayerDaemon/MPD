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

#include "config.h"
#include "SmbclientNeighborPlugin.hxx"
#include "lib/smbclient/Init.hxx"
#include "lib/smbclient/Domain.hxx"
#include "lib/smbclient/Mutex.hxx"
#include "neighbor/NeighborPlugin.hxx"
#include "neighbor/Explorer.hxx"
#include "neighbor/Listener.hxx"
#include "neighbor/Info.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "thread/Thread.hxx"
#include "thread/Name.hxx"
#include "util/Macros.hxx"
#include "util/Domain.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <libsmbclient.h>

#include <list>
#include <algorithm>

class SmbclientNeighborExplorer final : public NeighborExplorer {
	struct Server {
		std::string name, comment;

		bool alive;

		Server(std::string &&_name, std::string &&_comment)
			:name(std::move(_name)), comment(std::move(_comment)),
			 alive(true) {}
		Server(const Server &) = delete;

		gcc_pure
		bool operator==(const Server &other) const {
			return name == other.name;
		}

		gcc_pure
		NeighborInfo Export() const {
			return { "smb://" + name + "/", comment };
		}
	};

	Thread thread;

	mutable Mutex mutex;
	Cond cond;

	List list;

	bool quit;

public:
	SmbclientNeighborExplorer(NeighborListener &_listener)
		:NeighborExplorer(_listener) {}

	/* virtual methods from class NeighborExplorer */
	virtual bool Open(Error &error) override;
	virtual void Close() override;
	virtual List GetList() const override;

private:
	void Run();
	void ThreadFunc();
	static void ThreadFunc(void *ctx);
};

bool
SmbclientNeighborExplorer::Open(Error &error)
{
	quit = false;
	return thread.Start(ThreadFunc, this, error);
}

void
SmbclientNeighborExplorer::Close()
{
	mutex.lock();
	quit = true;
	cond.signal();
	mutex.unlock();

	thread.Join();
}

NeighborExplorer::List
SmbclientNeighborExplorer::GetList() const
{
	const ScopeLock protect(mutex);
	/*
	List list;
	for (const auto &i : servers)
		list.emplace_front(i.Export());
	*/
	return list;
}

static void
ReadServer(NeighborExplorer::List &list, const smbc_dirent &e)
{
	const std::string name(e.name, e.namelen);
	const std::string comment(e.comment, e.commentlen);

	list.emplace_front("smb://" + name, name + " (" + comment + ")");
}

static void
ReadServers(NeighborExplorer::List &list, const char *uri);

static void
ReadWorkgroup(NeighborExplorer::List &list, const std::string &name)
{
	std::string uri = "smb://" + name;
	ReadServers(list, uri.c_str());
}

static void
ReadEntry(NeighborExplorer::List &list, const smbc_dirent &e)
{
	switch (e.smbc_type) {
	case SMBC_WORKGROUP:
		ReadWorkgroup(list, std::string(e.name, e.namelen));
		break;

	case SMBC_SERVER:
		ReadServer(list, e);
		break;
	}
}

static void
ReadServers(NeighborExplorer::List &list, int fd)
{
	smbc_dirent *e;
	while ((e = smbc_readdir(fd)) != nullptr)
		ReadEntry(list, *e);

	smbc_closedir(fd);
}

static void
ReadServers(NeighborExplorer::List &list, const char *uri)
{
	int fd = smbc_opendir(uri);
	if (fd >= 0) {
		ReadServers(list, fd);
		smbc_closedir(fd);
	} else
		FormatErrno(smbclient_domain, "smbc_opendir('%s') failed",
			    uri);
}

gcc_pure
static NeighborExplorer::List
DetectServers()
{
	NeighborExplorer::List list;
	const ScopeLock protect(smbclient_mutex);
	ReadServers(list, "smb://");
	return list;
}

gcc_pure
static NeighborExplorer::List::const_iterator
FindBeforeServerByURI(NeighborExplorer::List::const_iterator prev,
		      NeighborExplorer::List::const_iterator end,
		      const std::string &uri)
{
	for (auto i = std::next(prev); i != end; prev = i, i = std::next(prev))
		if (i->uri == uri)
			return prev;

	return end;
}

inline void
SmbclientNeighborExplorer::Run()
{
	List found = DetectServers(), lost;

	mutex.lock();

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
#if defined(__clang__) || GCC_CHECK_VERSION(4,7)
			lost.splice_after(lost.before_begin(), list, prev);
#else
			/* the forward_list::splice_after() lvalue
			   reference overload is missing in gcc 4.6 */
			lost.emplace_front(std::move(*i));
			list.erase_after(prev);
#endif
		}
	}

	for (auto prev = found_before_begin, i = std::next(prev);
	     i != found_end; prev = i, i = std::next(prev))
		list.push_front(*i);

	mutex.unlock();

	for (auto &i : lost)
		listener.LostNeighbor(i);

	for (auto &i : found)
		listener.FoundNeighbor(i);
}

inline void
SmbclientNeighborExplorer::ThreadFunc()
{
	mutex.lock();

	while (!quit) {
		mutex.unlock();

		Run();

		mutex.lock();
		if (quit)
			break;

		// TODO: sleep for how long?
		cond.timed_wait(mutex, 10000);
	}

	mutex.unlock();
}

void
SmbclientNeighborExplorer::ThreadFunc(void *ctx)
{
	SetThreadName("smbclient");

	SmbclientNeighborExplorer &e = *(SmbclientNeighborExplorer *)ctx;
	e.ThreadFunc();
}

static NeighborExplorer *
smbclient_neighbor_create(gcc_unused EventLoop &loop,
			  NeighborListener &listener,
			  gcc_unused const config_param &param,
			  gcc_unused Error &error)
{
	if (!SmbclientInit(error))
		return nullptr;

	return new SmbclientNeighborExplorer(listener);
}

const NeighborPlugin smbclient_neighbor_plugin = {
	"smbclient",
	smbclient_neighbor_create,
};
