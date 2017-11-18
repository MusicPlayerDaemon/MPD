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
#include "NfsStorage.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "storage/MemoryDirectoryReader.hxx"
#include "lib/nfs/Blocking.hxx"
#include "lib/nfs/Base.hxx"
#include "lib/nfs/Lease.hxx"
#include "lib/nfs/Connection.hxx"
#include "lib/nfs/Glue.hxx"
#include "fs/AllocatedPath.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "event/Loop.hxx"
#include "event/Call.hxx"
#include "event/DeferEvent.hxx"
#include "event/TimerEvent.hxx"
#include "util/StringCompare.hxx"

extern "C" {
#include <nfsc/libnfs.h>
#include <nfsc/libnfs-raw-nfs.h>
}

#include <string>

#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>

class NfsStorage final
	: public Storage, NfsLease {

	enum class State {
		INITIAL, CONNECTING, READY, DELAY,
	};

	const std::string base;

	const std::string server, export_name;

	NfsConnection *connection;

	DeferEvent defer_connect;
	TimerEvent reconnect_timer;

	Mutex mutex;
	Cond cond;
	State state = State::INITIAL;
	std::exception_ptr last_exception;

public:
	NfsStorage(EventLoop &_loop, const char *_base,
		   std::string &&_server, std::string &&_export_name)
		:base(_base),
		 server(std::move(_server)),
		 export_name(std::move(_export_name)),
		 defer_connect(_loop, BIND_THIS_METHOD(OnDeferredConnect)),
		 reconnect_timer(_loop, BIND_THIS_METHOD(OnReconnectTimer)) {
		nfs_init(_loop);
	}

	~NfsStorage() {
		BlockingCall(GetEventLoop(), [this](){ Disconnect(); });
		nfs_finish();
	}

	/* virtual methods from class Storage */
	StorageFileInfo GetInfo(const char *uri_utf8, bool follow) override;

	StorageDirectoryReader *OpenDirectory(const char *uri_utf8) override;

	std::string MapUTF8(const char *uri_utf8) const noexcept override;

	const char *MapToRelativeUTF8(const char *uri_utf8) const noexcept override;

	/* virtual methods from NfsLease */
	void OnNfsConnectionReady() noexcept final {
		assert(state == State::CONNECTING);

		SetState(State::READY);
	}

	void OnNfsConnectionFailed(std::exception_ptr e) noexcept final {
		assert(state == State::CONNECTING);

		SetState(State::DELAY, std::move(e));
		reconnect_timer.Schedule(std::chrono::minutes(1));
	}

	void OnNfsConnectionDisconnected(std::exception_ptr e) noexcept final {
		assert(state == State::READY);

		SetState(State::DELAY, std::move(e));
		reconnect_timer.Schedule(std::chrono::seconds(5));
	}

	/* DeferEvent callback */
	void OnDeferredConnect() noexcept {
		if (state == State::INITIAL)
			Connect();
	}

	/* callback for #reconnect_timer */
	void OnReconnectTimer() {
		assert(state == State::DELAY);

		Connect();
	}

private:
	EventLoop &GetEventLoop() noexcept {
		return defer_connect.GetEventLoop();
	}

	void SetState(State _state) noexcept {
		assert(GetEventLoop().IsInside());

		const std::lock_guard<Mutex> protect(mutex);
		state = _state;
		cond.broadcast();
	}

	void SetState(State _state, std::exception_ptr &&e) noexcept {
		assert(GetEventLoop().IsInside());

		const std::lock_guard<Mutex> protect(mutex);
		state = _state;
		last_exception = std::move(e);
		cond.broadcast();
	}

	void Connect() noexcept {
		assert(state != State::READY);
		assert(GetEventLoop().IsInside());

		connection = &nfs_get_connection(server.c_str(),
						 export_name.c_str());
		connection->AddLease(*this);

		SetState(State::CONNECTING);
	}

	void EnsureConnected() noexcept {
		if (state != State::READY)
			Connect();
	}

	void WaitConnected() {
		const std::lock_guard<Mutex> protect(mutex);

		while (true) {
			switch (state) {
			case State::INITIAL:
				/* schedule connect */
				mutex.unlock();
				defer_connect.Schedule();
				mutex.lock();
				if (state == State::INITIAL)
					cond.wait(mutex);
				break;

			case State::CONNECTING:
			case State::READY:
				return;

			case State::DELAY:
				assert(last_exception);
				std::rethrow_exception(last_exception);
			}
		}
	}

	void Disconnect() noexcept {
		assert(GetEventLoop().IsInside());

		switch (state) {
		case State::INITIAL:
			defer_connect.Cancel();
			break;

		case State::CONNECTING:
		case State::READY:
			connection->RemoveLease(*this);
			SetState(State::INITIAL);
			break;

		case State::DELAY:
			reconnect_timer.Cancel();
			SetState(State::INITIAL);
			break;
		}
	}
};

static std::string
UriToNfsPath(const char *_uri_utf8)
{
	assert(_uri_utf8 != nullptr);

	/* libnfs paths must begin with a slash */
	std::string uri_utf8("/");
	uri_utf8.append(_uri_utf8);

	return AllocatedPath::FromUTF8Throw(uri_utf8.c_str()).Steal();
}

std::string
NfsStorage::MapUTF8(const char *uri_utf8) const noexcept
{
	assert(uri_utf8 != nullptr);

	if (StringIsEmpty(uri_utf8))
		return base;

	return PathTraitsUTF8::Build(base.c_str(), uri_utf8);
}

const char *
NfsStorage::MapToRelativeUTF8(const char *uri_utf8) const noexcept
{
	return PathTraitsUTF8::Relative(base.c_str(), uri_utf8);
}

static void
Copy(StorageFileInfo &info, const struct stat &st) noexcept
{
	if (S_ISREG(st.st_mode))
		info.type = StorageFileInfo::Type::REGULAR;
	else if (S_ISDIR(st.st_mode))
		info.type = StorageFileInfo::Type::DIRECTORY;
	else
		info.type = StorageFileInfo::Type::OTHER;

	info.size = st.st_size;
	info.mtime = std::chrono::system_clock::from_time_t(st.st_mtime);
	info.device = st.st_dev;
	info.inode = st.st_ino;
}

class NfsGetInfoOperation final : public BlockingNfsOperation {
	const char *const path;
	StorageFileInfo info;

public:
	NfsGetInfoOperation(NfsConnection &_connection, const char *_path)
		:BlockingNfsOperation(_connection), path(_path) {}

	const StorageFileInfo &GetInfo() const {
		return info;
	}

protected:
	void Start() override {
		connection.Stat(path, *this);
	}

	void HandleResult(gcc_unused unsigned status, void *data) noexcept override {
		Copy(info, *(const struct stat *)data);
	}
};

StorageFileInfo
NfsStorage::GetInfo(const char *uri_utf8, gcc_unused bool follow)
{
	const std::string path = UriToNfsPath(uri_utf8);

	WaitConnected();

	NfsGetInfoOperation operation(*connection, path.c_str());
	operation.Run();
	return operation.GetInfo();
}

gcc_pure
static bool
SkipNameFS(const char *name) noexcept
{
	return name[0] == '.' &&
		(name[1] == 0 ||
		 (name[1] == '.' && name[2] == 0));
}

static void
Copy(StorageFileInfo &info, const struct nfsdirent &ent)
{
	switch (ent.type) {
	case NF3REG:
		info.type = StorageFileInfo::Type::REGULAR;
		break;

	case NF3DIR:
		info.type = StorageFileInfo::Type::DIRECTORY;
		break;

	default:
		info.type = StorageFileInfo::Type::OTHER;
		break;
	}

	info.size = ent.size;
	info.mtime = std::chrono::system_clock::from_time_t(ent.mtime.tv_sec);
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
	void Start() override {
		connection.OpenDirectory(path, *this);
	}

	void HandleResult(gcc_unused unsigned status,
			  void *data) noexcept override {
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
NfsStorage::OpenDirectory(const char *uri_utf8)
{
	const std::string path = UriToNfsPath(uri_utf8);

	WaitConnected();

	NfsListDirectoryOperation operation(*connection, path.c_str());
	operation.Run();

	return operation.ToReader();
}

static Storage *
CreateNfsStorageURI(EventLoop &event_loop, const char *base)
{
	if (strncmp(base, "nfs://", 6) != 0)
		return nullptr;

	const char *p = base + 6;

	const char *mount = strchr(p, '/');
	if (mount == nullptr)
		throw std::runtime_error("Malformed nfs:// URI");

	const std::string server(p, mount);

	nfs_set_base(server.c_str(), mount);

	return new NfsStorage(event_loop, base, server.c_str(), mount);
}

const StoragePlugin nfs_storage_plugin = {
	"nfs",
	CreateNfsStorageURI,
};
