// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_LOG_LEVEL_HXX
#define MPD_LOG_LEVEL_HXX

#ifdef _WIN32
#include <windows.h>
/* damn you, windows.h! */
#ifdef ERROR
#undef ERROR
#endif
#endif

enum class LogLevel {
	/**
	 * Debug message for developers.
	 */
	DEBUG,

	/**
	 * Unimportant informational message.
	 */
	INFO,

	/**
	 * Interesting informational message.
	 */
	NOTICE,

	/**
	 * Warning: something may be wrong.
	 */
	WARNING,

	/**
	 * An error has occurred, an operation could not finish
	 * successfully.
	 */
	ERROR,
};

#endif
