// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Close.hxx"
#include "Queue.hxx"
#include "io/FileDescriptor.hxx"

namespace Uring {

void
Close(Queue *queue, FileDescriptor fd) noexcept
{
	if (auto *s = queue != nullptr ? queue->GetSubmitEntry() : nullptr) {
		io_uring_prep_close(s, fd.Get());
		io_uring_sqe_set_data(s, nullptr);
		io_uring_sqe_set_flags(s, IOSQE_CQE_SKIP_SUCCESS);
		queue->Submit();
	} else {
		/* io_uring not available or queue full: fall back to
		   the classic close() system call */
		fd.Close();
	}
}

} // namespace Uring
