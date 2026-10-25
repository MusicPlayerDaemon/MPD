// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Callback.hxx"
#include "Lease.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"

#include <exception>

class NfsConnection;

/**
 * Utility class to implement a blocking NFS call using the libnfs
 * async API.  The actual method call is deferred to the #EventLoop
 * thread by Begin(), and method Wait() waits for completion.
 */
class BlockingNfsOperation : protected NfsCallback, NfsLease {
	static constexpr std::chrono::steady_clock::duration timeout =
		std::chrono::minutes(1);

	Mutex mutex;
	Cond cond;

	/**
	 * Has Start() registered this object as #NfsCallback?  Only
	 * accessed from the #EventLoop thread.
	 */
	bool started = false;

	bool finished = false;

	std::exception_ptr error;

	NfsConnection *connection;

public:
	/**
	 * Start the operation on the given connection.  The
	 * connection does not need to be ready yet.
	 *
	 * Must be called from within the #EventLoop thread.
	 */
	void Begin(NfsConnection &_connection) noexcept;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Wait();

private:
	bool LockWaitFinished() noexcept {
		std::unique_lock lock{mutex};
		return cond.wait_for(lock, timeout, [this]{ return finished; });
	}

	/**
	 * Mark the operation as "finished" and wake up the waiting
	 * thread.
	 */
	void LockSetFinished() noexcept {
		const std::lock_guard protect{mutex};
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
	virtual void Start(NfsConnection &c) = 0;
	virtual void HandleResult(NfsConnection &c, unsigned status, void *data) noexcept = 0;
};
