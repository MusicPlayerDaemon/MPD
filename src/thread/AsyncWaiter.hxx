/*
 * Copyright 2021 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
