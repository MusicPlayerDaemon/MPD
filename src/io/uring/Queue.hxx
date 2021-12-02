/*
 * Copyright 2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#pragma once

#include "Ring.hxx"
#include "util/IntrusiveList.hxx"

#include <liburing.h>

namespace Uring {

class Operation;
class CancellableOperation;

/**
 * High-level C++ wrapper for a `struct io_uring`.  It supports a
 * handler class, cancellation, ...
 */
class Queue {
	Ring ring;

	IntrusiveList<CancellableOperation> operations;

public:
	Queue(unsigned entries, unsigned flags);
	~Queue() noexcept;

	FileDescriptor GetFileDescriptor() const noexcept {
		return ring.GetFileDescriptor();
	}

	struct io_uring_sqe *GetSubmitEntry() noexcept {
		return ring.GetSubmitEntry();
	}

	/**
	 * Like GetSubmitEntry(), but call Submit() if the submit
	 * queue is full.
	 *
	 * May throw exceptions if Submit() fails.
	 */
	struct io_uring_sqe &RequireSubmitEntry();

	bool HasPending() const noexcept {
		return !operations.empty();
	}

protected:
	void AddPending(struct io_uring_sqe &sqe,
			Operation &operation) noexcept;

public:
	virtual void Push(struct io_uring_sqe &sqe,
			  Operation &operation) noexcept {
		AddPending(sqe, operation);
		Submit();
	}

	void Submit() {
		ring.Submit();
	}

	bool DispatchOneCompletion();

	void DispatchCompletions() {
		while (DispatchOneCompletion()) {}
	}

	bool WaitDispatchOneCompletion();

	void WaitDispatchCompletions() {
		while (WaitDispatchOneCompletion()) {}
	}

private:
	void DispatchOneCompletion(struct io_uring_cqe &cqe) noexcept;
};

} // namespace Uring
