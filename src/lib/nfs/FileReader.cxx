// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FileReader.hxx"
#include "Glue.hxx"
#include "Base.hxx"
#include "Connection.hxx"
#include "Context.hxx"
#include "event/Call.hxx"
#include "util/StringCompare.hxx"

#include <nfsc/libnfs.h> // for struct nfs_stat_64

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h> // for S_ISREG()

using std::string_view_literals::operator""sv;

/**
 * Check if the given "nfs://" URI is inside the export of a NFS
 * storage (see nfs_add_base()).  Wrapper for nfs_check_base() which
 * extracts from an absolute NFS file URI.
 */
static NfsCheckBaseResult
CheckBase(const char *uri) noexcept
{
	uri = StringAfterPrefixIgnoreCase(uri, "nfs://"sv);
	if (uri == nullptr)
		return {};

	const char *slash = std::strchr(uri, '/');
	if (slash == nullptr)
		return {};

	const std::string_view server{uri, slash};
	auto result = nfs_check_base(server, slash);
	if (result.relative_path == nullptr || *result.relative_path != '/')
		/* not inside a storage, or this is the export
		   itself (not a file inside it) */
		return {};

	return result;
}

NfsFileReader::NfsFileReader() noexcept
	:defer_open(nfs_get_event_loop(), BIND_THIS_METHOD(OnDeferredOpen))
{
}

NfsFileReader::~NfsFileReader() noexcept
{
	assert(state == State::INITIAL);
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

	context = NfsContext{NfsContext::Create{}};

	if (const auto result = CheckBase(uri); result.relative_path != nullptr) {
		/* this file is inside the export of a NFS storage:
		   parse the storage's URL (with its arguments), so
		   OnDeferredOpen() finds the storage's connection
		   (or creates a new one with the same arguments);
		   the path relative to the export is taken
		   verbatim, i.e. it is not parsed as an URL */
		nfs_url = context.ParseUrlDir(result.base_url.c_str());
		assert(nfs_url->file == nullptr);

		path = result.relative_path;
	} else {
		nfs_url = context.ParseUrlFull(uri);
		assert(nfs_url->file != nullptr);

		path = nfs_url->file;
	}

	/* the previous connection may have been destroyed after a
	   disconnect; look it up again in OnDeferredOpen() */
	connection = nullptr;

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
	read_buffer = std::make_unique_for_overwrite<std::byte[]>(size);

	try {
		connection->Read(fh, offset, {read_buffer.get(), size}, *this);
	} catch (...) {
		read_buffer.reset();
		throw;
	}
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
		connection->RemoveLease(*this);
		state = State::INITIAL;
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
		connection->RemoveLease(*this);
		connection->Close(fh);
		state = State::INITIAL;
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
		std::unreachable();

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
		std::unreachable();

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
#ifdef LIBNFS_API_2
		read_buffer.reset();
#endif
		state = State::IDLE;
		break;
	}

	OnNfsFileError(std::move(e));
}

void
NfsFileReader::OnDeferredOpen() noexcept
{
	assert(state == State::DEFER);
	assert(connection == nullptr);

	try {
		connection = &nfs_get_connection(std::move(context), *nfs_url, true);
	} catch (...) {
		state = State::INITIAL;
		OnNfsFileError(std::current_exception());
		return;
	}

	connection->AddLease(*this);
	state = State::MOUNT;
}
