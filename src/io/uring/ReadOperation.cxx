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
