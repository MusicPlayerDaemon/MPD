// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class FileDescriptor;

namespace Uring {

class Queue;

/**
 * Schedule a close() on the given file descriptor.  If no
 * #io_uring_sqe is available, this function falls back to close().
 * No callback will be invoked.
 */
void
Close(Queue *queue, FileDescriptor fd) noexcept;

} // namespace Uring
