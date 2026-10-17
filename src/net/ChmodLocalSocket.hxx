// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <sys/types.h> // for mode_t

/**
 * Call chmod() on a local socket.
 *
 * This can't be done with fchmod() on the #SocketDescriptor because
 * that affects the socket itself, not the inode in the filesystem.
 *
 * On Linux, this attempts to bring some safety against symlink
 * attacks: it opens an O_PATH #FileDescriptor (with O_NOFOLLOW),
 * checks if the file is really a socket, and only then calls
 * fchmodat() on that #FileDescriptor.
 *
 * Throws on error.
 */
void
ChmodLocalSocket(const char *path, mode_t mode);
