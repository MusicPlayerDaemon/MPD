/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_NFS_FILE_READER_HXX
#define MPD_NFS_FILE_READER_HXX

#include "Lease.hxx"
#include "Callback.hxx"
#include "event/InjectEvent.hxx"
#include "util/Compiler.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#include <sys/stat.h>

struct nfsfh;
class NfsConnection;

/**
 * A helper class which helps with reading from a file.  It obtains a
 * connection lease (#NfsLease), opens the given file, "stats" the
 * file, and finally allos you to read its contents.
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

	std::string server, export_name;
	const char *path;

	NfsConnection *connection;

	nfsfh *fh;

	/**
	 * To inject the Open() call into the I/O thread.
	 */
	InjectEvent defer_open;

public:
	NfsFileReader() noexcept;
	~NfsFileReader() noexcept;

	auto &GetEventLoop() const noexcept {
		return defer_open.GetEventLoop();
	}

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
	virtual void OnNfsFileRead(const void *data, size_t size) noexcept = 0;

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
	void StatCallback(const struct stat *st) noexcept;

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
