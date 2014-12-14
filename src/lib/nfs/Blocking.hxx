/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "check.h"
#include "Callback.hxx"
#include "Lease.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "util/Error.hxx"

class NfsConnection;

/**
 * Utility class to implement a blocking NFS call using the libnfs
 * async API.  The actual method call is deferred to the #EventLoop
 * thread, and method Run() waits for completion.
 */
class BlockingNfsOperation : protected NfsCallback, NfsLease {
	static constexpr unsigned timeout_ms = 60000;

	Mutex mutex;
	Cond cond;

	bool finished;

	Error error;

protected:
	NfsConnection &connection;

public:
	BlockingNfsOperation(NfsConnection &_connection)
		:finished(false), connection(_connection) {}

	bool Run(Error &error);

private:
	bool LockWaitFinished() {
		const ScopeLock protect(mutex);
		while (!finished)
			if (!cond.timed_wait(mutex, timeout_ms))
				return false;

		return true;
	}

	/**
	 * Mark the operation as "finished" and wake up the waiting
	 * thread.
	 */
	void LockSetFinished() {
		const ScopeLock protect(mutex);
		finished = true;
		cond.signal();
	}

	/* virtual methods from NfsLease */
	void OnNfsConnectionReady() final;
	void OnNfsConnectionFailed(const Error &error) final;
	void OnNfsConnectionDisconnected(const Error &error) final;

	/* virtual methods from NfsCallback */
	void OnNfsCallback(unsigned status, void *data) final;
	void OnNfsError(Error &&error) final;

protected:
	virtual bool Start(Error &error) = 0;
	virtual void HandleResult(unsigned status, void *data) = 0;
};

#endif
