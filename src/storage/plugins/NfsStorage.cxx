// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
#include "input/InputStream.hxx"
#include "input/plugins/NfsInputPlugin.hxx"
#include "fs/AllocatedPath.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "event/Loop.hxx"
#include "event/Call.hxx"
#include "event/InjectEvent.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/ASCII.hxx"
#include "util/StringCompare.hxx"

extern "C" {
#include <nfsc/libnfs.h>
#include <nfsc/libnfs-raw-nfs.h>
}

#include <fmt/core.h>

#include <cassert>
#include <string>

#include <sys/stat.h>
#include <fcntl.h>

using std::string_view_literals::operator""sv;

class NfsStorage final
	: public Storage, NfsLease {

	enum class State {
		INITIAL, CONNECTING, READY, DELAY,
	};

	/**
	 * The full configured URL (with all arguemnts).  This is used
	 * to reconnect.
	 */
	const std::string url;

	/**
	 * The base URL for building file URLs (without arguments).
	 */
	const std::string base;

	NfsConnection *connection;

	InjectEvent defer_connect;
	CoarseTimerEvent reconnect_timer;

	Mutex mutex;
	Cond cond;
	State state = State::CONNECTING;
	std::exception_ptr last_exception;

public:
	NfsStorage(const char *_url, NfsConnection &_connection)
		:url(_url),
		 base(fmt::format("nfs://{}{}"sv, _connection.GetServer(), _connection.GetExportName())),
		 connection(&_connection),
		 defer_connect(_connection.GetEventLoop(), BIND_THIS_METHOD(OnDeferredConnect)),
		 reconnect_timer(_connection.GetEventLoop(), BIND_THIS_METHOD(OnReconnectTimer))
	{
		BlockingCall(GetEventLoop(), [this](){
			connection->AddLease(*this);
		});
	}

	~NfsStorage() override {
		BlockingCall(GetEventLoop(), [this](){ Disconnect(); });
		nfs_finish();
	}

	NfsStorage(const NfsStorage &) = delete;
	NfsStorage &operator=(const NfsStorage &) = delete;

	/* virtual methods from class Storage */
	StorageFileInfo GetInfo(std::string_view uri_utf8, bool follow) override;

	std::unique_ptr<StorageDirectoryReader> OpenDirectory(std::string_view uri_utf8) override;

	[[nodiscard]] std::string MapUTF8(std::string_view uri_utf8) const noexcept override;

	[[nodiscard]] std::string_view MapToRelativeUTF8(std::string_view uri_utf8) const noexcept override;

	InputStreamPtr OpenFile(std::string_view uri_utf8, Mutex &mutex) override;

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

	/* InjectEvent callback */
	void OnDeferredConnect() noexcept {
		if (state == State::INITIAL)
			Connect();
	}

	/* callback for #reconnect_timer */
	void OnReconnectTimer() noexcept {
		assert(state == State::DELAY);

		Connect();
	}

private:
	[[nodiscard]] EventLoop &GetEventLoop() const noexcept {
		return defer_connect.GetEventLoop();
	}

	void SetState(State _state) noexcept {
		assert(GetEventLoop().IsInside());

		const std::scoped_lock protect{mutex};
		state = _state;
		cond.notify_all();
	}

	void SetState(State _state, std::exception_ptr &&e) noexcept {
		assert(GetEventLoop().IsInside());

		const std::scoped_lock protect{mutex};
		state = _state;
		last_exception = std::move(e);
		cond.notify_all();
	}

	void Connect() noexcept {
		assert(state != State::READY);
		assert(GetEventLoop().IsInside());

		try {
			connection = &nfs_make_connection(url.c_str());
		} catch (...) {
			SetState(State::DELAY, std::current_exception());
			reconnect_timer.Schedule(std::chrono::minutes(10));
			return;
		}

		connection->AddLease(*this);

		SetState(State::CONNECTING);
	}

	void WaitConnected() {
		std::unique_lock lock{mutex};

		while (true) {
			switch (state) {
			case State::INITIAL:
				/* schedule connect */
				{
					const ScopeUnlock unlock(mutex);
					defer_connect.Schedule();
				}

				if (state == State::INITIAL)
					cond.wait(lock);
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
		assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

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
UriToNfsPath(std::string_view _uri_utf8)
{
	/* libnfs paths must begin with a slash */
	std::string uri_utf8("/");
	uri_utf8.append(_uri_utf8);

#ifdef _WIN32
	/* assume UTF-8 when accessing NFS from Windows */
	return uri_utf8;
#else
	return AllocatedPath::FromUTF8Throw(uri_utf8).Steal();
#endif
}

std::string
NfsStorage::MapUTF8(std::string_view uri_utf8) const noexcept
{
	if (uri_utf8.empty())
		return base;

	return PathTraitsUTF8::Build(base, uri_utf8);
}

std::string_view
NfsStorage::MapToRelativeUTF8(std::string_view uri_utf8) const noexcept
{
	return PathTraitsUTF8::Relative(base, uri_utf8);
}

InputStreamPtr
NfsStorage::OpenFile(std::string_view uri_utf8, Mutex &_mutex)
{
	WaitConnected();

	return OpenNfsInputStream(*connection, uri_utf8, _mutex);
}

static void
Copy(StorageFileInfo &info, const struct nfs_stat_64 &st) noexcept
{
	if (S_ISREG(st.nfs_mode))
		info.type = StorageFileInfo::Type::REGULAR;
	else if (S_ISDIR(st.nfs_mode))
		info.type = StorageFileInfo::Type::DIRECTORY;
	else
		info.type = StorageFileInfo::Type::OTHER;

	info.size = st.nfs_size;
	info.mtime = std::chrono::system_clock::from_time_t(st.nfs_mtime);
	info.device = st.nfs_dev;
	info.inode = st.nfs_ino;
}

class NfsGetInfoOperation final : public BlockingNfsOperation {
	const char *const path;
	StorageFileInfo info;
	bool follow;

public:
	NfsGetInfoOperation(NfsConnection &_connection, const char *_path,
			    bool _follow)
		:BlockingNfsOperation(_connection), path(_path),
		 follow(_follow) {}

	[[nodiscard]] const StorageFileInfo &GetInfo() const {
		return info;
	}

protected:
	void Start() override {
		if (follow)
			connection.Stat(path, *this);
		else
			connection.Lstat(path, *this);
	}

	void HandleResult([[maybe_unused]] unsigned status, void *data) noexcept override {
		Copy(info, *(const struct nfs_stat_64 *)data);
	}
};

StorageFileInfo
NfsStorage::GetInfo(std::string_view uri_utf8, bool follow)
{
	const std::string path = UriToNfsPath(uri_utf8);

	WaitConnected();

	NfsGetInfoOperation operation(*connection, path.c_str(), follow);
	operation.Run();
	return operation.GetInfo();
}

[[gnu::pure]]
static bool
SkipNameFS(PathTraitsFS::const_pointer name) noexcept
{
	return PathTraitsFS::IsSpecialFilename(name);
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

	std::unique_ptr<StorageDirectoryReader> ToReader() {
		return std::make_unique<MemoryStorageDirectoryReader>(std::move(entries));
	}

protected:
	void Start() override {
		connection.OpenDirectory(path, *this);
	}

	void HandleResult([[maybe_unused]] unsigned status,
			  void *data) noexcept override {
		auto *const dir = (struct nfsdir *)data;

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
#ifdef _WIN32
		/* assume UTF-8 when accessing NFS from Windows */
		const auto name_fs = AllocatedPath::FromUTF8Throw(ent->name);
		if (name_fs.IsNull())
			continue;
#else
		const Path name_fs = Path::FromFS(ent->name);
#endif
		if (SkipNameFS(name_fs.c_str()))
			continue;

		try {
			entries.emplace_front(name_fs.ToUTF8Throw());
			Copy(entries.front().info, *ent);
		} catch (...) {
			/* ignore files whose name cannot be converted
			   to UTF-8 */
		}
	}
}

std::unique_ptr<StorageDirectoryReader>
NfsStorage::OpenDirectory(std::string_view uri_utf8)
{
	const std::string path = UriToNfsPath(uri_utf8);

	WaitConnected();

	NfsListDirectoryOperation operation(*connection, path.c_str());
	operation.Run();

	return operation.ToReader();
}

static std::unique_ptr<Storage>
CreateNfsStorageURI(EventLoop &event_loop, const char *base)
{
	if (!StringStartsWithCaseASCII(base, "nfs://"sv))
		return nullptr;

	nfs_init(event_loop);

	try {
		auto &connection = nfs_make_connection(base);
		nfs_set_base(connection.GetServer(), connection.GetExportName());

		return std::make_unique<NfsStorage>(base, connection);
	} catch (...) {
		nfs_finish();
		throw;
	}
}

static constexpr const char *nfs_prefixes[] = { "nfs://", nullptr };

const StoragePlugin nfs_storage_plugin = {
	"nfs",
	nfs_prefixes,
	CreateNfsStorageURI,
};
