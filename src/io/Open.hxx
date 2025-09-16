// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

class FileDescriptor;
class UniqueFileDescriptor;
struct FileAt;

UniqueFileDescriptor
OpenReadOnly(const char *path, int flags=0);

UniqueFileDescriptor
OpenWriteOnly(const char *path, int flags=0);

#ifndef _WIN32

UniqueFileDescriptor
OpenDirectory(const char *name, int flags=0);

#endif

#ifdef __linux__

UniqueFileDescriptor
OpenPath(const char *path, int flags=0);

UniqueFileDescriptor
OpenPath(FileAt file, int flags=0);

UniqueFileDescriptor
OpenReadOnly(FileAt file, int flags=0);

UniqueFileDescriptor
OpenWriteOnly(FileAt file, int flags=0);

UniqueFileDescriptor
OpenDirectory(FileAt file, int flags=0);

struct opwn_how;

/**
 * Wrapper for openat2() which converts the returned file descriptor
 * to a #UniqueFileDescriptor.
 *
 * Returns an "undefined" instance on error and sets errno.
 */
UniqueFileDescriptor
TryOpen(FileAt file, const struct open_how &how) noexcept;

/**
 * Wrapper for openat2() which converts the returned file descriptor
 * to a #UniqueFileDescriptor.
 *
 * Throws on error.
 */
UniqueFileDescriptor
Open(FileAt file, const struct open_how &how);

#endif
