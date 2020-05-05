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
	Ring(unsigned entries, unsigned flags);

	~Ring() noexcept {
		io_uring_queue_exit(&ring);
	}

	Ring(const Ring &) = delete;
	Ring &operator=(const Ring &) = delete;

	FileDescriptor GetFileDescriptor() const noexcept {
		return FileDescriptor(ring.ring_fd);
	}

	struct io_uring_sqe *GetSubmitEntry() noexcept {
		return io_uring_get_sqe(&ring);
	}

	void Submit();

	struct io_uring_cqe *WaitCompletion();

	/**
	 * @return a completion queue entry or nullptr on EAGAIN
	 */
	struct io_uring_cqe *PeekCompletion();

	void SeenCompletion(struct io_uring_cqe &cqe) noexcept {
		io_uring_cqe_seen(&ring, &cqe);
	}
};

} // namespace Uring
