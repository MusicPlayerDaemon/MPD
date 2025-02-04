// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Ring.hxx"
#include "util/IntrusiveList.hxx"

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
	Queue(unsigned entries, struct io_uring_params &params);
	~Queue() noexcept;

	FileDescriptor GetFileDescriptor() const noexcept {
		return ring.GetFileDescriptor();
	}

	void SetMaxWorkers(unsigned values[2]) {
		ring.SetMaxWorkers(values);
	}

	void SetMaxWorkers(unsigned bounded, unsigned unbounded) {
		ring.SetMaxWorkers(bounded, unbounded);
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

	void SubmitAndGetEvents() {
		ring.SubmitAndGetEvents();
	}

public:
	void Push(struct io_uring_sqe &sqe,
		  Operation &operation) noexcept {
		AddPending(sqe, operation);
		Submit();
	}

	virtual void Submit() {
		ring.Submit();
	}

	/**
	 * @return true if a completion was dispatched, false if the
	 * completion queue was empty
	 */
	bool DispatchOneCompletion();

	/**
	 * @return true if at least one completion was dispatched,
	 * false if the completion queue was empty
	 */
	bool DispatchCompletions() {
		bool result = false;
		while (DispatchOneCompletion()) {
			result = true;
		}
		return result;
	}

	/**
	 * @return true if a completion was dispatched, false if the
	 * completion queue was empty
	 */
	bool WaitDispatchOneCompletion();

	void WaitDispatchCompletions() {
		while (WaitDispatchOneCompletion()) {}
	}

	bool SubmitAndWaitDispatchOneCompletion(struct __kernel_timespec &timeout);

private:
	static void _DispatchOneCompletion(const struct io_uring_cqe &cqe) noexcept;
	void DispatchOneCompletion(struct io_uring_cqe &cqe) noexcept;

	/**
	 * Dispatch all completions using io_uring_for_each_cqe().
	 */
	unsigned DispatchCompletions(struct io_uring_cqe &cqe) noexcept;
};

} // namespace Uring
