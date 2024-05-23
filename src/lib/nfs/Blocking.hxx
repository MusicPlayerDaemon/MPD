// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
		std::unique_lock lock{mutex};
		return cond.wait_for(lock, timeout, [this]{ return finished; });
	}

	/**
	 * Mark the operation as "finished" and wake up the waiting
	 * thread.
	 */
	void LockSetFinished() noexcept {
		const std::scoped_lock protect{mutex};
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
