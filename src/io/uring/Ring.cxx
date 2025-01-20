// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Ring.hxx"
#include "system/Error.hxx"

namespace Uring {

Ring::Ring(unsigned entries, unsigned flags)
{
	if (int error = io_uring_queue_init(entries, &ring, flags);
	    error < 0)
		throw MakeErrno(-error, "io_uring_queue_init() failed");
}

Ring::Ring(unsigned entries, struct io_uring_params &params)
{
	if (int error = io_uring_queue_init_params(entries, &ring, &params);
	    error < 0)
		throw MakeErrno(-error, "io_uring_queue_init_params() failed");
}

void
Ring::Submit()
{
	if (int error = io_uring_submit(&ring);
	    error < 0)
		throw MakeErrno(-error, "io_uring_submit() failed");
}

void
Ring::SubmitAndGetEvents()
{
	if (int error = io_uring_submit_and_get_events(&ring);
	    error < 0)
		throw MakeErrno(-error, "io_uring_submit() failed");
}

struct io_uring_cqe *
Ring::WaitCompletion()
{
	struct io_uring_cqe *cqe;
	if (int error = io_uring_wait_cqe(&ring, &cqe);
	    error < 0) {
		if (error == -EAGAIN)
			return nullptr;

		throw MakeErrno(-error, "io_uring_wait_cqe() failed");
	}

	return cqe;
}

struct io_uring_cqe *
Ring::PeekCompletion()
{
	struct io_uring_cqe *cqe;
	if (int error = io_uring_peek_cqe(&ring, &cqe); error < 0) {
		if (error == -EAGAIN)
			return nullptr;

		throw MakeErrno(-error, "io_uring_peek_cqe() failed");
	}

	return cqe;
}

} // namespace Uring
