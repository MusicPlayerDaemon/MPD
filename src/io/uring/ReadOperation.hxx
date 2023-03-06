// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
