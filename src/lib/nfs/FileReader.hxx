// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NFS_FILE_READER_HXX
#define MPD_NFS_FILE_READER_HXX

#include "Lease.hxx"
#include "Callback.hxx"
#include "event/InjectEvent.hxx"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <string>

#ifdef LIBNFS_API_2
#include <memory>
#endif

struct nfsfh;
struct nfs_stat_64;
class NfsConnection;

/**
 * A helper class which helps with reading from a file.  It obtains a
 * connection lease (#NfsLease), opens the given file, "stats" the
 * file, and finally allows you to read its contents.
 *
 * To get started, derive your class from it and implement the pure
 * virtual methods, construct an instance, and call Open().
 */
class NfsFileReader : NfsLease, NfsCallback {
	enum class State {
		INITIAL,
		DEFER,
		MOUNT,
		OPEN,
		STAT,
		READ,
		IDLE,
	};

	State state = State::INITIAL;

	std::string server, export_name, path;

	NfsConnection *connection = nullptr;

	nfsfh *fh;

	/**
	 * To inject the Open() call into the I/O thread.
	 */
	InjectEvent defer_open;

#ifdef LIBNFS_API_2
	std::unique_ptr<std::byte[]> read_buffer;
#endif

public:
	NfsFileReader() noexcept;
	explicit NfsFileReader(NfsConnection &_connection,
			       std::string_view _path) noexcept;
	~NfsFileReader() noexcept;

	auto &GetEventLoop() const noexcept {
		return defer_open.GetEventLoop();
	}

	[[nodiscard]] [[gnu::pure]]
	std::string GetAbsoluteUri() const noexcept;

	void Close() noexcept;
	void DeferClose() noexcept;

	/**
	 * Open the file.  This method is thread-safe.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Open(const char *uri);

	/**
	 * Attempt to read from the file.  This may only be done after
	 * OnNfsFileOpen() has been called.  Only one read operation
	 * may be performed at a time.
	 *
	 * This method is not thread-safe and must be called from
	 * within the I/O thread.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Read(uint64_t offset, size_t size);

	/**
	 * Cancel the most recent Read() call.
	 *
	 * This method is not thread-safe and must be called from
	 * within the I/O thread.
	 */
	void CancelRead() noexcept;

	bool IsIdle() const noexcept {
		return state == State::IDLE;
	}

protected:
	/**
	 * The file has been opened successfully.  It is a regular
	 * file, and its size is known.  It is ready to be read from
	 * using Read().
	 *
	 * This method will be called from within the I/O thread.
	 */
	virtual void OnNfsFileOpen(uint64_t size) noexcept = 0;

	/**
	 * A Read() has completed successfully.
	 *
	 * This method will be called from within the I/O thread.
	 */
	virtual void OnNfsFileRead(std::span<const std::byte> src) noexcept = 0;

	/**
	 * An error has occurred, which can be either while waiting
	 * for OnNfsFileOpen(), or while waiting for OnNfsFileRead(),
	 * or if disconnected while idle.
	 */
	virtual void OnNfsFileError(std::exception_ptr &&e) noexcept = 0;

private:
	/**
	 * Cancel the current operation, if any.  The NfsLease must be
	 * unregistered already.
	 */
	void CancelOrClose() noexcept;

	void OpenCallback(nfsfh *_fh) noexcept;
	void StatCallback(const struct nfs_stat_64 *st) noexcept;
	void ReadCallback(std::size_t nbytes, const void *data) noexcept;

	/* virtual methods from NfsLease */
	void OnNfsConnectionReady() noexcept final;
	void OnNfsConnectionFailed(std::exception_ptr e) noexcept final;
	void OnNfsConnectionDisconnected(std::exception_ptr e) noexcept final;

	/* virtual methods from NfsCallback */
	void OnNfsCallback(unsigned status, void *data) noexcept final;
	void OnNfsError(std::exception_ptr &&e) noexcept final;

	/* InjectEvent callback */
	void OnDeferredOpen() noexcept;
};

#endif
