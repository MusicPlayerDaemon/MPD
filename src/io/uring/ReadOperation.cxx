// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "ReadOperation.hxx"
#include "Queue.hxx"
#include "io/FileDescriptor.hxx"

#include <cassert>

namespace Uring {

void
ReadOperation::Start(Queue &queue, FileDescriptor fd, off_t offset,
		     std::size_t size, ReadHandler &_handler) noexcept
{
	assert(!buffer);

	handler = &_handler;

	buffer = std::make_unique<std::byte[]>(size);

	auto &s = queue.RequireSubmitEntry();

	iov.iov_base = buffer.get();
	iov.iov_len = size;

	io_uring_prep_readv(&s, fd.Get(), &iov, 1, offset);
	queue.Push(s, *this);
}

void
ReadOperation::OnUringCompletion(int res) noexcept
{
	if (handler == nullptr)
		/* operation was canceled */
		delete this;
	else if (res >= 0)
		handler->OnRead(std::move(buffer), res);
	else
		handler->OnReadError(-res);
}

} // namespace Uring
