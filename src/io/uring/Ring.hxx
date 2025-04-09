// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/FileDescriptor.hxx"

#include <liburing.h>

namespace Uring {

/**
 * Low-level C++ wrapper for a `struct io_uring`.  It provides simple
 * wrappers to liburing functions and throws std::system_error on
 * errors.
 */
class Ring {
	struct io_uring ring;

// io_uring_setup() flags supported by Linux kernel 5.6
unsigned iouring_setup_mask = (
	IORING_SETUP_IOPOLL |
	IORING_SETUP_SQPOLL |
	IORING_SETUP_SQ_AFF |
	IORING_SETUP_CQSIZE |
	IORING_SETUP_CLAMP |
	IORING_SETUP_ATTACH_WQ
	);

public:
	/**
	 * Construct the io_uring using io_uring_queue_init().
	 *
	 * Throws on error.
	 */
	Ring(unsigned entries, unsigned flags);

	/**
	 * Construct the io_uring using io_uring_queue_init().
	 *
	 * Throws on error.
	 *
	 * @param params initialization parameters; will also be
	 * written to by this constructor
	 */
	Ring(unsigned entries, struct io_uring_params &params);

	~Ring() noexcept {
		io_uring_queue_exit(&ring);
	}

	Ring(const Ring &) = delete;
	Ring &operator=(const Ring &) = delete;

	/**
	 * Returns the io_uring file descriptor.
	 */
	FileDescriptor GetFileDescriptor() const noexcept {
		return FileDescriptor(ring.ring_fd);
	}

	/**
	 * Wrapper for io_uring_register_iowq_max_workers().
	 *
	 * Throws on error.
	 */
	void SetMaxWorkers(unsigned values[2]);

	/**
	 * This overload constructs an array for
	 * io_uring_register_iowq_max_workers() and discards the
	 * output values.
	 */
	void SetMaxWorkers(unsigned bounded, unsigned unbounded) {
		unsigned values[2] = {bounded, unbounded};
		SetMaxWorkers(values);
	}

	/**
	 * Returns a submit queue entry or nullptr if the submit queue
	 * is full.
	 */
	struct io_uring_sqe *GetSubmitEntry() noexcept {
		return io_uring_get_sqe(&ring);
	}

	/**
	 * Submit all pending entries from the submit queue to the
	 * kernel using io_uring_submit().
	 *
	 * Throws on error.
	 *
	 * @see io_uring_submit()
	 */
	void Submit();

	/**
	 * Like Submit(), but also flush completions.
	 *
	 * @see io_uring_submit_and_get_events()
	 */
	void SubmitAndGetEvents();

	/**
	 * Waits for one completion.
	 *
	 * Throws on error.
	 *
	 * @return a completion queue entry or nullptr on EAGAIN
	 */
	struct io_uring_cqe *WaitCompletion();

	/**
	 * Submit requests and wait for one completion (or a timeout).
	 * Wrapper for io_uring_submit_and_wait_timeout().
	 *
	 * Throws on error.
	 *
	 * @return a completion queue entry or nullptr on EAGAIN/ETIME
	 */
	struct io_uring_cqe *SubmitAndWaitCompletion(struct __kernel_timespec *timeout);

	/**
	 * Peek one completion (non-blocking).
	 *
	 * Throws on error.
	 *
	 * @return a completion queue entry or nullptr on EAGAIN
	 */
	struct io_uring_cqe *PeekCompletion();

	/**
	 * Mark one completion event as consumed.
	 */
	void SeenCompletion(struct io_uring_cqe &cqe) noexcept {
		io_uring_cqe_seen(&ring, &cqe);
	}

	/**
	 * Invoke a function with a reference to each completion (but
	 * do not mark it "seen" or advance the completion queue
	 * head).
	 */
	unsigned ForEachCompletion(struct io_uring_cqe *cqe,
				   std::invocable<struct io_uring_cqe &> auto f) noexcept {
		unsigned dummy, n = 0;

		io_uring_for_each_cqe(&ring, dummy, cqe) {
			++n;
			f(*cqe);
		}

		return n;
	}

	/**
	 * Like ForEachCompletion(), but advance the completion queue
	 * head.
	 */
	unsigned VisitCompletions(struct io_uring_cqe *cqe,
				  std::invocable<const struct io_uring_cqe &> auto f) noexcept {
		unsigned n = ForEachCompletion(cqe, f);
		if (n > 0)
			io_uring_cq_advance(&ring, n);
		return n;
	}
};

} // namespace Uring
