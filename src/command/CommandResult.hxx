// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_COMMAND_RESULT_HXX
#define MPD_COMMAND_RESULT_HXX

#ifdef _WIN32
#include <windows.h>
/* damn you, windows.h! */
#ifdef ERROR
#undef ERROR
#endif
#endif

enum class CommandResult {
	/**
	 * The command has succeeded, but the "OK" response was not
	 * yet sent to the client.
	 */
	OK,

	/**
	 * The connection is now in "idle" mode, and no response shall
	 * be generated.
	 */
	IDLE,

	/**
	 * A #BackgroundCommand has been installed.
	 */
	BACKGROUND,

	/**
	 * There was an error.  The "ACK" response was sent to the
	 * client.
	 */
	ERROR,

	/**
	 * The client has asked MPD to close the connection.  MPD will
	 * flush the remaining output buffer first.
	 */
	FINISH,

	/**
	 * The connection to this client shall be closed.
	 */
	CLOSE,

	/**
	 * The MPD process shall be shut down.
	 */
	KILL,
};

#endif
