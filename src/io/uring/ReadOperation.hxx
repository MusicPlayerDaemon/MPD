/*
 * Copyright 2020 Max Kellermann <max.kellermann@gmail.com>
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

#include "Operation.hxx"

#include <cstddef>
#include <memory>

#include <sys/uio.h> // for struct iovec

class FileDescriptor;

namespace Uring {

class Queue;

class ReadHandler {
public:
	virtual void OnRead(std::unique_ptr<std::byte[]> buffer,
			    std::size_t size) noexcept = 0;

	/**
	 * @param error an errno value
	 */
	virtual void OnReadError(int error) noexcept = 0;
};

/**
 * Read into a newly allocated buffer.
 *
 * Instances of this class must be allocated with `new`, because
 * cancellation will require this object (and the allocated buffer) to
 * persist until the kernel completes the operation.
 */
class ReadOperation final : Operation {
	ReadHandler *handler;

	struct iovec iov;

	std::unique_ptr<std::byte[]> buffer;

public:
	void Start(Queue &queue, FileDescriptor fd, off_t offset,
		   std::size_t size, ReadHandler &_handler) noexcept;

	/**
	 * Cancel this operation.  This instance will be freed using
	 * `delete` after the kernel has finished cancellation,
	 * i.e. the caller resigns ownership.
	 */
	void Cancel() noexcept {
		handler = nullptr;
	}

private:
	/* virtual methods from class Operation */
	void OnUringCompletion(int res) noexcept override;
};

} // namespace Uring
