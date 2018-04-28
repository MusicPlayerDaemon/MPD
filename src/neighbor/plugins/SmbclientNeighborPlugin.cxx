/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Log.hxx"

#include <libsmbclient.h>

#include <list>
#include <algorithm>

#include "util/Domain.hxx"
#include "Log.hxx"

#include "Instance.hxx"
#include "player/Control.hxx"
#include "Partition.hxx"
#include <unistd.h>

//This value determines the maximum number of local master browsers to query for the list of workgroups
#define MAX_LMB_COUNT  10

extern Instance *instance;

static constexpr Domain smbclient_neighbor("smbclient_neighbor");

class SmbclientNeighborExplorer final : public NeighborExplorer {
	struct Server {
		std::string name, comment, workgroup;

		bool alive;

		Server(std::string &&_name, std::string &&_comment, std::string &&_workgroup)
			:name(std::move(_name)), comment(std::move(_comment)), workgroup(_workgroup),
			 alive(true) {}
		Server(const Server &) = delete;

		gcc_pure
		bool operator==(const Server &other) const noexcept {
			return name == other.name;
		}

		gcc_pure
		NeighborInfo Export() const noexcept {
			return { "smb://" + name + "/", comment,"", workgroup};
		}
	};

	Thread thread;

	mutable Mutex mutex;
	Cond cond;

	List list;

	bool quit;

public:
	SmbclientNeighborExplorer(NeighborListener &_listener)
		:NeighborExplorer(_listener),
		 thread(BIND_THIS_METHOD(ThreadFunc)) {}

	/* virtual methods from class NeighborExplorer */
	void Open() override;
	void Reopen() override;
	void Close() noexcept override;
	List GetList() const noexcept override;

private:
	void Run();
	void ThreadFunc();
};

void
SmbclientNeighborExplorer::Open()
{
	quit = false;
	list.clear();
	thread.Start();
}

void
SmbclientNeighborExplorer::Reopen()
{
	FormatDefault(smbclient_neighbor, "%s %d", __func__, __LINE__);
	Close();
	SmbclientReinit();
	Open();
}

void
SmbclientNeighborExplorer::Close() noexcept
{
	mutex.lock();
	quit = true;
	cond.signal();
	mutex.unlock();

	thread.Join();
}

NeighborExplorer::List
SmbclientNeighborExplorer::GetList() const noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	/*
	List list;
	for (const auto &i : servers)
		list.emplace_front(i.Export());
	*/
	return list;
}

static void
ReadServer(NeighborExplorer::List &list, const smbc_dirent &e, const char *wg)
{
	const std::string name(e.name, e.namelen);
	std::string comment(e.comment, e.commentlen);

	if (comment.empty() || comment.compare("")==0) {
		comment = name;
	}
	list.emplace_front("smb://" + name, comment, "", wg);
}

static void
ReadServers(NeighborExplorer::List &list, const char *uri, const char *wg);

static void
ReadWorkgroup(NeighborExplorer::List &list, const std::string &name)
{
	std::string uri = "smb://" + name;
	ReadServers(list, uri.c_str(), name.c_str());
}

static void
ReadEntry(NeighborExplorer::List &list, const smbc_dirent &e, const char *wg)
{
	switch (e.smbc_type) {
	case SMBC_WORKGROUP:
		ReadWorkgroup(list, std::string(e.name, e.namelen));
		break;

	case SMBC_SERVER:
		ReadServer(list, e, wg);
		break;
	}
}

static void
ReadServers(NeighborExplorer::List &list, int fd, const char *wg)
{
	smbc_dirent *e;
	while ((e = smbc_readdir(fd)) != nullptr)
		ReadEntry(list, *e, wg);

	smbc_closedir(fd);
}

static void
ReadServers(NeighborExplorer::List &list, const char *uri, const char *wg)
{
	int fd = smbc_opendir(uri);
	if (fd >= 0) {
		ReadServers(list, fd, wg);
		smbc_closedir(fd);
	} else
		FormatErrno(smbclient_domain, "smbc_opendir('%s') failed",
			    uri);
}

gcc_pure
static NeighborExplorer::List
DetectServers() noexcept
{
	NeighborExplorer::List list;
	const std::lock_guard<Mutex> protect(smbclient_mutex);
	ReadServers(list, "smb://", "");
	return list;
}

inline void
SmbclientNeighborExplorer::Run()
{
	List found = DetectServers(), found2;

	mutex.lock();


	for (auto &i : found) {
		bool is_in_list = false;
		for (auto &j : list) {
			if (i.uri == j.uri) {
				is_in_list = true;
				break;
			}
		}
		if (!is_in_list) {
			found2.push_front(i);
		}
	}
	for (auto &i : found2) {
		list.push_front(i);
	}

	mutex.unlock();

	for (auto &i : found2) {
		listener.FoundNeighbor(i);
		FormatDefault(smbclient_neighbor, "FoundNeighbor\n workgroup:%s\n uri:%s\n display_name:%s",
			i.workgroup.c_str(), i.uri.c_str(), i.display_name.c_str());
	}
}

inline void
SmbclientNeighborExplorer::ThreadFunc()
{
	SetThreadName("smbclientNeighbor");

	mutex.lock();
	scanning = 10;

	do {
		mutex.unlock();
		//FormatDefault(smbclient_neighbor, "%s start", __func__);
		Run();
		//FormatDefault(smbclient_neighbor, "%s end", __func__);

		mutex.lock();
		if (quit)
			break;

		// TODO: sleep for how long?
		scanning--;
		cond.timed_wait(mutex, std::chrono::seconds(5));
	} while (!quit && scanning);
	scanning = 0;

	mutex.unlock();
}

static std::unique_ptr<NeighborExplorer>
smbclient_neighbor_create(gcc_unused EventLoop &loop,
			  NeighborListener &listener,
			  gcc_unused const ConfigBlock &block)
{
	SmbclientInit();

	return std::make_unique<SmbclientNeighborExplorer>(listener);
}

const NeighborPlugin smbclient_neighbor_plugin = {
	"smbclient",
	smbclient_neighbor_create,
};
