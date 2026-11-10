// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef INPUT_ERROR_HXX
#define INPUT_ERROR_HXX

#include <exception>

/**
 * Was this exception thrown because the requested file does not
 * exist?  This function attempts to recognize exceptions thrown by
 * various input plugins.
 */
[[gnu::pure]]
bool
IsFileNotFound(std::exception_ptr e) noexcept;

/**
 * Was this exception caused by a server which could not be reached
 * at all (e.g. DNS failure, connection refused, timeout)?  This
 * returns false for errors which prove that the server is alive,
 * e.g. "file not found" or any HTTP status error.
 */
[[gnu::pure]]
bool
IsUnreachable(std::exception_ptr e) noexcept;

#endif
