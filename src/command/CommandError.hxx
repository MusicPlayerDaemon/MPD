// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_COMMAND_ERROR_HXX
#define MPD_COMMAND_ERROR_HXX

#include <exception>

class Response;

/**
 * Send the exception to the client.
 */
void
PrintError(Response &r, const std::exception_ptr& ep);

#endif
