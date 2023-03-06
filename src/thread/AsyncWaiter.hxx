// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef ASYNC_WAITER_HXX
#define ASYNC_WAITER_HXX

#include "Mutex.hxx"
#include "Cond.hxx"

#include <exception>

/**
 * A helper class which can be used to implement asynchronous
 * operations which can be waited on.  Errors are rethrown into the
 * waiting thread.
 */
class AsyncWaiter {
	mutable Mutex mutex;
	Cond cond;

	std::exception_ptr error;

	bool done = false;

public:
	bool IsDone() const noexcept {
		const std::scoped_lock lock{mutex};
		return done;
	}

	void Wait() {
		std::unique_lock lock(mutex);
		cond.wait(lock, [this]{ return done; });

		if (error)
			std::rethrow_exception(error);
	}

	void SetDone() noexcept {
		const std::scoped_lock lock{mutex};
		done = true;
		cond.notify_one();
	}

	void SetError(std::exception_ptr e) noexcept {
		const std::scoped_lock lock{mutex};
		error = std::move(e);
		done = true;
		cond.notify_one();
	}
};

#endif
