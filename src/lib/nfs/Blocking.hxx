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

#ifndef MPD_BLOCKING_NFS_CALLBACK_HXX
#define MPD_BLOCKING_NFS_CALLBACK_HXX

#include "Callback.hxx"
#include "Lease.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"

#include <exception>

class NfsConnection;

/**
 * Utility class to implement a blocking NFS call using the libnfs
 * async API.  The actual method call is deferred to the #EventLoop
 * thread, and method Run() waits for completion.
 */
class BlockingNfsOperation : protected NfsCallback, NfsLease {
	static constexpr std::chrono::steady_clock::duration timeout =
		std::chrono::minutes(1);

	Mutex mutex;
	Cond cond;

	bool finished;

	std::exception_ptr error;

protected:
	NfsConnection &connection;

public:
	BlockingNfsOperation(NfsConnection &_connection) noexcept
		:finished(false), connection(_connection) {}

	/**
	 * Throws std::runtime_error on error.
	 */
	void Run();

private:
	bool LockWaitFinished() noexcept {
		std::unique_lock<Mutex> lock(mutex);
		return cond.wait_for(lock, timeout, [this]{ return finished; });
	}

	/**
	 * Mark the operation as "finished" and wake up the waiting
	 * thread.
	 */
	void LockSetFinished() noexcept {
		const std::scoped_lock<Mutex> protect(mutex);
		finished = true;
		cond.notify_one();
	}

	/* virtual methods from NfsLease */
	void OnNfsConnectionReady() noexcept final;
	void OnNfsConnectionFailed(std::exception_ptr e) noexcept final;
	void OnNfsConnectionDisconnected(std::exception_ptr e) noexcept final;

	/* virtual methods from NfsCallback */
	void OnNfsCallback(unsigned status, void *data) noexcept final;
	void OnNfsError(std::exception_ptr &&e) noexcept final;

protected:
	virtual void Start() = 0;
	virtual void HandleResult(unsigned status, void *data) noexcept = 0;
};

#endif
