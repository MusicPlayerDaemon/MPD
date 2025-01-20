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

public:
	/**
	 * Construct the io_uring using io_uring_queue_init().
	 *
	 * Throws on error.
	 */
	Ring(unsigned entries, unsigned flags);

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
};

} // namespace Uring
