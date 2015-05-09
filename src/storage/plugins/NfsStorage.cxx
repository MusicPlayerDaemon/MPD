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
#include "NfsStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "storage/MemoryDirectoryReader.hxx"
#include "lib/nfs/Blocking.hxx"
#include "lib/nfs/Domain.hxx"
#include "lib/nfs/Base.hxx"
#include "lib/nfs/Lease.hxx"
#include "lib/nfs/Connection.hxx"
#include "lib/nfs/Glue.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/Error.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "event/Loop.hxx"
#include "event/Call.hxx"
#include "event/DeferredMonitor.hxx"
#include "event/TimeoutMonitor.hxx"

extern "C" {
#include <nfsc/libnfs.h>
#include <nfsc/libnfs-raw-nfs.h>
}

#include <string>

#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>

class NfsStorage final
	: public Storage, NfsLease, DeferredMonitor, TimeoutMonitor {

	enum class State {
		INITIAL, CONNECTING, READY, DELAY,
	};

	const std::string base;

	const std::string server, export_name;

	NfsConnection *connection;

	Mutex mutex;
	Cond cond;
	State state;
	Error last_error;

public:
	NfsStorage(EventLoop &_loop, const char *_base,
		   std::string &&_server, std::string &&_export_name)
		:DeferredMonitor(_loop), TimeoutMonitor(_loop),
		 base(_base),
		 server(std::move(_server)),
		 export_name(std::move(_export_name)),
		 state(State::INITIAL) {
		nfs_init();
	}

	~NfsStorage() {
		BlockingCall(GetEventLoop(), [this](){ Disconnect(); });
		nfs_finish();
	}

	/* virtual methods from class Storage */
	bool GetInfo(const char *uri_utf8, bool follow, FileInfo &info,
		     Error &error) override;

	StorageDirectoryReader *OpenDirectory(const char *uri_utf8,
					      Error &error) override;

	std::string MapUTF8(const char *uri_utf8) const override;

	const char *MapToRelativeUTF8(const char *uri_utf8) const override;

	/* virtual methods from NfsLease */
	void OnNfsConnectionReady() final {
		assert(state == State::CONNECTING);

		SetState(State::READY);
	}

	void OnNfsConnectionFailed(gcc_unused const Error &error) final {
		assert(state == State::CONNECTING);

		SetState(State::DELAY, error);
		TimeoutMonitor::ScheduleSeconds(60);
	}

	void OnNfsConnectionDisconnected(gcc_unused const Error &error) final {
		assert(state == State::READY);

		SetState(State::DELAY, error);
		TimeoutMonitor::ScheduleSeconds(5);
	}

	/* virtual methods from DeferredMonitor */
	void RunDeferred() final {
		if (state == State::INITIAL)
			Connect();
	}

	/* virtual methods from TimeoutMonitor */
	void OnTimeout() final {
		assert(state == State::DELAY);

		Connect();
	}

private:
	EventLoop &GetEventLoop() {
		return DeferredMonitor::GetEventLoop();
	}

	void SetState(State _state) {
		assert(GetEventLoop().IsInside());

		const ScopeLock protect(mutex);
		state = _state;
		cond.broadcast();
	}

	void SetState(State _state, const Error &error) {
		assert(GetEventLoop().IsInside());

		const ScopeLock protect(mutex);
		state = _state;
		last_error.Clear();
		last_error.Set(error);
		cond.broadcast();
	}

	void Connect() {
		assert(state != State::READY);
		assert(GetEventLoop().IsInside());

		connection = &nfs_get_connection(server.c_str(),
						 export_name.c_str());
		connection->AddLease(*this);

		SetState(State::CONNECTING);
	}

	void EnsureConnected() {
		if (state != State::READY)
			Connect();
	}

	bool WaitConnected(Error &error) {
		const ScopeLock protect(mutex);

		while (true) {
			switch (state) {
			case State::INITIAL:
				/* schedule connect */
				mutex.unlock();
				DeferredMonitor::Schedule();
				mutex.lock();
				if (state == State::INITIAL)
					cond.wait(mutex);
				break;

			case State::CONNECTING:
			case State::READY:
				return true;

			case State::DELAY:
				assert(last_error.IsDefined());
				error.Set(last_error);
				return false;
			}
		}
	}

	void Disconnect() {
		assert(GetEventLoop().IsInside());

		switch (state) {
		case State::INITIAL:
			DeferredMonitor::Cancel();
			break;

		case State::CONNECTING:
		case State::READY:
			connection->RemoveLease(*this);
			SetState(State::INITIAL);
			break;

		case State::DELAY:
			TimeoutMonitor::Cancel();
			SetState(State::INITIAL);
			break;
		}
	}
};

static std::string
UriToNfsPath(const char *_uri_utf8, Error &error)
{
	assert(_uri_utf8 != nullptr);

	/* libnfs paths must begin with a slash */
	std::string uri_utf8("/");
	uri_utf8.append(_uri_utf8);

	return AllocatedPath::FromUTF8(uri_utf8.c_str(), error).Steal();
}

std::string
NfsStorage::MapUTF8(const char *uri_utf8) const
{
	assert(uri_utf8 != nullptr);

	if (*uri_utf8 == 0)
		return base;

	return PathTraitsUTF8::Build(base.c_str(), uri_utf8);
}

const char *
NfsStorage::MapToRelativeUTF8(const char *uri_utf8) const
{
	return PathTraitsUTF8::Relative(base.c_str(), uri_utf8);
}

static void
Copy(FileInfo &info, const struct stat &st)
{
	if (S_ISREG(st.st_mode))
		info.type = FileInfo::Type::REGULAR;
	else if (S_ISDIR(st.st_mode))
		info.type = FileInfo::Type::DIRECTORY;
	else
		info.type = FileInfo::Type::OTHER;

	info.size = st.st_size;
	info.mtime = st.st_mtime;
	info.device = st.st_dev;
	info.inode = st.st_ino;
}

class NfsGetInfoOperation final : public BlockingNfsOperation {
	const char *const path;
	FileInfo &info;

public:
	NfsGetInfoOperation(NfsConnection &_connection, const char *_path,
			    FileInfo &_info)
		:BlockingNfsOperation(_connection), path(_path), info(_info) {}

protected:
	bool Start(Error &_error) override {
		return connection.Stat(path, *this, _error);
	}

	void HandleResult(gcc_unused unsigned status, void *data) override {
		Copy(info, *(const struct stat *)data);
	}
};

bool
NfsStorage::GetInfo(const char *uri_utf8, gcc_unused bool follow,
		    FileInfo &info, Error &error)
{
	const std::string path = UriToNfsPath(uri_utf8, error);
	if (path.empty())
		return false;

	if (!WaitConnected(error))
		return false;

	NfsGetInfoOperation operation(*connection, path.c_str(), info);
	return operation.Run(error);
}

gcc_pure
static bool
SkipNameFS(const char *name)
{
	return name[0] == '.' &&
		(name[1] == 0 ||
		 (name[1] == '.' && name[2] == 0));
}

static void
Copy(FileInfo &info, const struct nfsdirent &ent)
{
	switch (ent.type) {
	case NF3REG:
		info.type = FileInfo::Type::REGULAR;
		break;

	case NF3DIR:
		info.type = FileInfo::Type::DIRECTORY;
		break;

	default:
		info.type = FileInfo::Type::OTHER;
		break;
	}

	info.size = ent.size;
	info.mtime = ent.mtime.tv_sec;
	info.device = 0;
	info.inode = ent.inode;
}

class NfsListDirectoryOperation final : public BlockingNfsOperation {
	const char *const path;

	MemoryStorageDirectoryReader::List entries;

public:
	NfsListDirectoryOperation(NfsConnection &_connection,
				  const char *_path)
		:BlockingNfsOperation(_connection), path(_path) {}

	StorageDirectoryReader *ToReader() {
		return new MemoryStorageDirectoryReader(std::move(entries));
	}

protected:
	bool Start(Error &_error) override {
		return connection.OpenDirectory(path, *this, _error);
	}

	void HandleResult(gcc_unused unsigned status, void *data) override {
		struct nfsdir *const dir = (struct nfsdir *)data;

		CollectEntries(dir);
		connection.CloseDirectory(dir);
	}

private:
	void CollectEntries(struct nfsdir *dir);
};

inline void
NfsListDirectoryOperation::CollectEntries(struct nfsdir *dir)
{
	assert(entries.empty());

	const struct nfsdirent *ent;
	while ((ent = connection.ReadDirectory(dir)) != nullptr) {
		const Path name_fs = Path::FromFS(ent->name);
		if (SkipNameFS(name_fs.c_str()))
			continue;

		std::string name_utf8 = name_fs.ToUTF8();
		if (name_utf8.empty())
			/* ignore files whose name cannot be converted
			   to UTF-8 */
			continue;

		entries.emplace_front(std::move(name_utf8));
		Copy(entries.front().info, *ent);
	}
}

StorageDirectoryReader *
NfsStorage::OpenDirectory(const char *uri_utf8, Error &error)
{
	const std::string path = UriToNfsPath(uri_utf8, error);
	if (path.empty())
		return nullptr;

	if (!WaitConnected(error))
		return nullptr;

	NfsListDirectoryOperation operation(*connection, path.c_str());
	if (!operation.Run(error))
		return nullptr;

	return operation.ToReader();
}

static Storage *
CreateNfsStorageURI(EventLoop &event_loop, const char *base,
		    Error &error)
{
	if (memcmp(base, "nfs://", 6) != 0)
		return nullptr;

	const char *p = base + 6;

	const char *mount = strchr(p, '/');
	if (mount == nullptr) {
		error.Set(nfs_domain, "Malformed nfs:// URI");
		return nullptr;
	}

	const std::string server(p, mount);

	nfs_set_base(server.c_str(), mount);

	return new NfsStorage(event_loop, base, server.c_str(), mount);
}

const StoragePlugin nfs_storage_plugin = {
	"nfs",
	CreateNfsStorageURI,
};
