/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
