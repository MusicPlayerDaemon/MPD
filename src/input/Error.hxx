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

#endif
