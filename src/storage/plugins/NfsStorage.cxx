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
#include "lib/nfs/Context.hxx"
#include "lib/nfs/Url.hxx"
#include "lib/nfs/Glue.hxx"
#include "input/InputStream.hxx"
#include "input/plugins/NfsInputPlugin.hxx"
#include "fs/AllocatedPath.hxx"
#include "event/Loop.hxx"
#include "event/Call.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/StringCompare.hxx"

extern "C" {
#include <nfsc/libnfs.h>
#include <nfsc/libnfs-raw-nfs.h>
}

#include <fmt/format.h>

#include <cassert>
#include <string>

#include <sys/stat.h>
#include <fcntl.h>

using std::string_view_literals::operator""sv;

static constexpr std::size_t MAX_NFS_DIRECTORY_ENTRIES = 256 * 1024;

class NfsStorage final
	: public Storage, NfsLease {

	enum class State {
		/**
		 * There is no #NfsConnection currently (because it
		 * failed or Disconnect() was called).  Most likely a
		 * new #NfsConnection will eventually be obtained by
		 * Connect(), triggered by #reconnect_timer.
		 *
		 * In this state, #last_exception may be set while
		 * #reconnect_timer is pending.
		 */
		INITIAL,

		/**
		 * We have an #NfsConnection, but it is not yet ready.
		 * Waiting for the OnNfsConnectionReady() callback
		 * which will set state to #READY.
		 */
		CONNECTING,

		/**
		 * We have an #NfsConnection and it is ready and
		 * usable.
		 */
		READY,
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

	CoarseTimerEvent reconnect_timer;

	State state = State::CONNECTING;
	std::exception_ptr last_exception;

public:
	NfsStorage(std::string_view _url, NfsConnection &_connection)
		:url(_url),
		 base(fmt::format("nfs://{}{}"sv, _connection.GetServer(), _connection.GetExportName())),
		 connection(&_connection),
		 reconnect_timer(_connection.GetEventLoop(), BIND_THIS_METHOD(OnReconnectTimer))
	{
		assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

		connection->AddLease(*this);

		nfs_add_base(this, url, _connection.GetServer(),
			     _connection.GetExportName());
	}

	~NfsStorage() override {
		nfs_remove_base(this);
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

		ScheduleReconnectTimer(std::move(e), std::chrono::minutes{1});
	}

	void OnNfsConnectionDisconnected(std::exception_ptr e) noexcept final {
		assert(state == State::READY);

		ScheduleReconnectTimer(std::move(e), std::chrono::seconds{5});
	}

	/* callback for #reconnect_timer */
	void OnReconnectTimer() noexcept {
		assert(state == State::INITIAL);
		assert(last_exception);

		last_exception = {};

		Connect();
	}

private:
	[[nodiscard]] EventLoop &GetEventLoop() const noexcept {
		return reconnect_timer.GetEventLoop();
	}

	void SetState(State _state) noexcept {
		assert(GetEventLoop().IsInside());

		state = _state;
	}

	void SetState(State _state, std::exception_ptr &&e) noexcept {
		assert(GetEventLoop().IsInside());

		state = _state;
		last_exception = std::move(e);
	}

	void ScheduleReconnectTimer(std::exception_ptr &&e,
				    Event::Duration delay) noexcept {
		assert(GetEventLoop().IsInside());
		assert(!last_exception);
		assert(!reconnect_timer.IsPending());

		SetState(State::INITIAL, std::move(e));
		reconnect_timer.Schedule(delay);
	}

	void Connect() noexcept {
		assert(state != State::READY);
		assert(GetEventLoop().IsInside());

		try {
			NfsContext context{NfsContext::Create{}};
			const auto nfs_url = context.ParseUrlDir(url.c_str());
			connection = &nfs_get_connection(std::move(context), *nfs_url, false);
		} catch (...) {
			ScheduleReconnectTimer(std::current_exception(), std::chrono::minutes{10});
			return;
		}

		connection->AddLease(*this);

		SetState(State::CONNECTING);
	}

	/**
	 * Ensure that we have a valid #NfsConnection instance (which
	 * may not yet be ready though).  Throws on error.
	 */
	void EnsureConnection() {
		assert(GetEventLoop().IsInside());

		switch (state) {
		case State::INITIAL:
			if (last_exception) [[unlikely]]
				std::rethrow_exception(last_exception);

			Connect();

			if (last_exception) [[unlikely]]
				/* Connect() has failed and #connection
				   is not valid */
				std::rethrow_exception(last_exception);

			break;

		case State::CONNECTING:
			/* we have a NfsConnection, but it's not yet
			   ready - good enough */
			break;

		case State::READY:
			break;
		}
	}

	void Begin(BlockingNfsOperation &operation) {
		operation.Begin(*connection);
	}

	void Run(BlockingNfsOperation &operation) {
		BlockingCall(GetEventLoop(), [this, &operation](){
			EnsureConnection();
			Begin(operation);
		});

		operation.Wait();
	}

	void Disconnect() noexcept {
		assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

		switch (state) {
		case State::INITIAL:
			reconnect_timer.Cancel();
			last_exception = {};
			break;

		case State::CONNECTING:
		case State::READY:
			connection->RemoveLease(*this);
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
		/* this special case returns the original "nfs://" URI
                   with all parameters (they are missing in the "base"
                   variable); this is important because MapUTF8("") is
                   called for the state file */
		return url;

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
	/* make sure our connection exists, so NfsFileReader finds it
	   (via nfs_check_base()), or fail early if it is currently
	   unavailable */
	BlockingCall(GetEventLoop(), [this](){
		EnsureConnection();
	});

	/* open by URI; NfsFileReader looks up the connection in the
	   I/O thread, because our "connection" pointer may be
	   invalidated by the I/O thread at any time */
	return OpenNfsInputStream(MapUTF8(uri_utf8), _mutex);
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
	NfsGetInfoOperation(const char *_path, bool _follow) noexcept
		:path(_path), follow(_follow) {}

	[[nodiscard]] const StorageFileInfo &GetInfo() const {
		return info;
	}

protected:
	void Start(NfsConnection &c) override {
		if (follow)
			c.Stat(path, *this);
		else
			c.Lstat(path, *this);
	}

	void HandleResult(NfsConnection &,
			  [[maybe_unused]] unsigned status, void *data) noexcept override {
		Copy(info, *(const struct nfs_stat_64 *)data);
	}
};

StorageFileInfo
NfsStorage::GetInfo(std::string_view uri_utf8, bool follow)
{
	const std::string path = UriToNfsPath(uri_utf8);

	NfsGetInfoOperation operation{path.c_str(), follow};
	Run(operation);
	return operation.GetInfo();
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
	explicit NfsListDirectoryOperation(const char *_path)
		:path(_path) {}

	std::unique_ptr<StorageDirectoryReader> ToReader() {
		return std::make_unique<MemoryStorageDirectoryReader>(std::move(entries));
	}

protected:
	void Start(NfsConnection &c) override {
		c.OpenDirectory(path, *this);
	}

	void HandleResult(NfsConnection &c,
			  [[maybe_unused]] unsigned status,
			  void *data) noexcept override {
		auto *const dir = (struct nfsdir *)data;

		CollectEntries(c, dir);
		c.CloseDirectory(dir);
	}

private:
	void CollectEntries(NfsConnection &c, struct nfsdir *dir);
};

inline void
NfsListDirectoryOperation::CollectEntries(NfsConnection &c, struct nfsdir *dir)
{
	assert(entries.empty());

	std::size_t n = 0;
	const struct nfsdirent *ent;
	while ((ent = c.ReadDirectory(dir)) != nullptr) {
		if (!PathTraitsUTF8::IsValidFilename(ent->name))
			continue;

#ifdef _WIN32
		/* assume UTF-8 when accessing NFS from Windows */
		const auto name_fs = AllocatedPath::FromUTF8(ent->name);
		if (name_fs.IsNull())
			continue;
#else
		const Path name_fs = Path::FromFS(ent->name);
#endif
		try {
			entries.emplace_front(name_fs.ToUTF8Throw());
			Copy(entries.front().info, *ent);

			if (++n >= MAX_NFS_DIRECTORY_ENTRIES)
				/* too many already - stop here */
				break;
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

	NfsListDirectoryOperation operation{path.c_str()};
	Run(operation);
	return operation.ToReader();
}

static std::unique_ptr<Storage>
_CreateNfsStorageURI(EventLoop &event_loop, std::string_view base)
{
	assert(StringStartsWithIgnoreCase(base, "nfs://"sv));

	nfs_init(event_loop);

	try {
		NfsContext context{NfsContext::Create{}};
		const auto nfs_url = context.ParseUrlDir(std::string{base}.c_str());

		auto &connection = nfs_get_connection(std::move(context), *nfs_url, false);

		return std::make_unique<NfsStorage>(base, connection);
	} catch (...) {
		nfs_finish();
		throw;
	}
}

static std::unique_ptr<Storage>
CreateNfsStorageURI(EventLoop &event_loop, std::string_view base)
{
	if (!StringStartsWithIgnoreCase(base, "nfs://"sv))
		return nullptr;

	std::unique_ptr<Storage> result;

	BlockingCall(event_loop, [&result, &event_loop, base]{
		result = _CreateNfsStorageURI(event_loop, base);
	});

	assert(result);
	return result;
}

static constexpr const char *nfs_prefixes[] = { "nfs://", nullptr };

const StoragePlugin nfs_storage_plugin = {
	"nfs",
	nfs_prefixes,
	CreateNfsStorageURI,
};
