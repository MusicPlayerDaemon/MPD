// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FileReader.hxx"
#include "Glue.hxx"
#include "Base.hxx"
#include "Connection.hxx"
#include "event/Call.hxx"
#include "util/ASCII.hxx"

#include <nfsc/libnfs.h> // for struct nfs_stat_64

#include <fmt/core.h>

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h> // for S_ISREG()

using std::string_view_literals::operator""sv;

NfsFileReader::NfsFileReader() noexcept
	:defer_open(nfs_get_event_loop(), BIND_THIS_METHOD(OnDeferredOpen))
{
}

NfsFileReader::NfsFileReader(NfsConnection &_connection,
			     std::string_view _path) noexcept
	:state(State::DEFER),
	 path(_path),
	 connection(&_connection),
	 defer_open(_connection.GetEventLoop(), BIND_THIS_METHOD(OnDeferredOpen))
{
	defer_open.Schedule();
}

NfsFileReader::~NfsFileReader() noexcept
{
	assert(state == State::INITIAL);
}

std::string
NfsFileReader::GetAbsoluteUri() const noexcept
{
	return fmt::format("nfs://{}{}/{}"sv,
			   connection != nullptr ? connection->GetServer() : server,
			   connection != nullptr ? connection->GetExportName() : export_name,
			   path);
}

void
NfsFileReader::Close() noexcept
{
	if (state == State::INITIAL)
		return;

	if (state == State::DEFER) {
		state = State::INITIAL;
		defer_open.Cancel();
		return;
	}

	/* this cancels State::MOUNT */
	connection->RemoveLease(*this);

	CancelOrClose();
}

void
NfsFileReader::CancelOrClose() noexcept
{
	assert(state != State::INITIAL &&
	       state != State::DEFER);

	if (state == State::IDLE)
		/* no async operation in progress: can close
		   immediately */
		connection->Close(fh);
	else if (state > State::OPEN) {
		/* one async operation in progress: cancel it and
		   defer the nfs_close_async() call */
		DisposablePointer dispose_value{};

#ifdef LIBNFS_API_2
		dispose_value = ToDeleteArray(read_buffer.release());
#endif

		connection->Cancel(*this, fh, std::move(dispose_value));
	} else if (state > State::MOUNT)
		/* we don't have a file handle yet - just cancel the
		   async operation */
		connection->Cancel(*this, nullptr, {});

	state = State::INITIAL;
}

void
NfsFileReader::DeferClose() noexcept
{
	BlockingCall(GetEventLoop(), [this](){ Close(); });
}

void
NfsFileReader::Open(const char *uri)
{
	assert(state == State::INITIAL);

	if (!StringStartsWithCaseASCII(uri, "nfs://"))
		throw std::runtime_error("Malformed nfs:// URI");

	uri += 6;

	const char *slash = std::strchr(uri, '/');
	if (slash == nullptr)
		throw std::runtime_error("Malformed nfs:// URI");

	server = std::string(uri, slash);

	uri = slash;

	const char *new_path = nfs_check_base(server.c_str(), uri);
	if (new_path != nullptr) {
		export_name = std::string(uri, new_path);
		if (*new_path == 0)
			new_path = "/";
		path = new_path;
	} else {
		slash = std::strrchr(uri + 1, '/');
		if (slash == nullptr || slash[1] == 0)
			throw std::runtime_error("Malformed nfs:// URI");

		export_name = std::string(uri, slash);
		path = slash;
	}

	state = State::DEFER;
	defer_open.Schedule();
}

void
NfsFileReader::Read(uint64_t offset, size_t size)
{
	assert(state == State::IDLE);

#ifdef LIBNFS_API_2
	assert(!read_buffer);
	// TOOD read into caller-provided buffer
	read_buffer = std::make_unique<std::byte[]>(size);
	connection->Read(fh, offset, {read_buffer.get(), size}, *this);
#else
	connection->Read(fh, offset, size, *this);
#endif

	state = State::READ;
}

void
NfsFileReader::CancelRead() noexcept
{
	if (state == State::READ) {
		DisposablePointer dispose_value{};

#ifdef LIBNFS_API_2
		assert(read_buffer);
		dispose_value = ToDeleteArray(read_buffer.release());
#endif

		connection->Cancel(*this, nullptr, std::move(dispose_value));
		state = State::IDLE;
	}
}

void
NfsFileReader::OnNfsConnectionReady() noexcept
{
	assert(state == State::MOUNT);

	try {
		connection->Open(path.c_str(), O_RDONLY, *this);
	} catch (...) {
		OnNfsFileError(std::current_exception());
		return;
	}

	state = State::OPEN;
}

void
NfsFileReader::OnNfsConnectionFailed(std::exception_ptr e) noexcept
{
	assert(state == State::MOUNT);

	state = State::INITIAL;

	OnNfsFileError(std::move(e));
}

void
NfsFileReader::OnNfsConnectionDisconnected(std::exception_ptr e) noexcept
{
	assert(state > State::MOUNT);

	CancelOrClose();

	OnNfsFileError(std::move(e));
}

inline void
NfsFileReader::OpenCallback(nfsfh *_fh) noexcept
{
	assert(connection != nullptr);
	assert(_fh != nullptr);

	fh = _fh;

	try {
		connection->Stat(fh, *this);
	} catch (...) {
		OnNfsFileError(std::current_exception());
		return;
	}

	state = State::STAT;
}

inline void
NfsFileReader::StatCallback(const struct nfs_stat_64 *st) noexcept
{
	assert(connection != nullptr);
	assert(fh != nullptr);
	assert(st != nullptr);

	if (!S_ISREG(st->nfs_mode)) {
		OnNfsFileError(std::make_exception_ptr(std::runtime_error("Not a regular file")));
		return;
	}

	OnNfsFileOpen(st->nfs_size);
}

inline void
NfsFileReader::ReadCallback(std::size_t nbytes, const void *data) noexcept
{
#ifdef LIBNFS_API_2
	(void)data;

	assert(read_buffer);
	const auto buffer = std::move(read_buffer);

	OnNfsFileRead({buffer.get(), nbytes});
#else
	OnNfsFileRead({static_cast<const std::byte *>(data), nbytes});
#endif
}

void
NfsFileReader::OnNfsCallback(unsigned status, void *data) noexcept
{
	switch (std::exchange(state, State::IDLE)) {
	case State::INITIAL:
	case State::DEFER:
	case State::MOUNT:
	case State::IDLE:
		assert(false);
		gcc_unreachable();

	case State::OPEN:
		OpenCallback((struct nfsfh *)data);
		break;

	case State::STAT:
		StatCallback((const struct nfs_stat_64 *)data);
		break;

	case State::READ:
		ReadCallback(static_cast<std::size_t>(status), data);
		break;
	}
}

void
NfsFileReader::OnNfsError(std::exception_ptr &&e) noexcept
{
	switch (state) {
	case State::INITIAL:
	case State::DEFER:
	case State::MOUNT:
	case State::IDLE:
		assert(false);
		gcc_unreachable();

	case State::OPEN:
		connection->RemoveLease(*this);
		state = State::INITIAL;
		break;

	case State::STAT:
		connection->RemoveLease(*this);
		connection->Close(fh);
		state = State::INITIAL;
		break;

	case State::READ:
		state = State::IDLE;
		break;
	}

	OnNfsFileError(std::move(e));
}

void
NfsFileReader::OnDeferredOpen() noexcept
{
	assert(state == State::DEFER);

	if (connection == nullptr)
		try {
			connection = &nfs_get_connection(server, export_name);
		} catch (...) {
			OnNfsFileError(std::current_exception());
			return;
		}

	connection->AddLease(*this);
	state = State::MOUNT;
}
