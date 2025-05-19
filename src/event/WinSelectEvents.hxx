// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <windows.h>

/* ERROR is a WIN32 macro that poisons our namespace; this is a kludge
   to allow us to use it anyway */
#ifdef ERROR
#undef ERROR
#endif

struct WinSelectEvents {
	static constexpr unsigned READ = 1;
	static constexpr unsigned WRITE = 2;
	static constexpr unsigned ERROR = 0;
	static constexpr unsigned HANGUP = 0;

	/**
	 * A mask containing all events which indicate a dead socket
	 * connection (i.e. error or hangup).  It may still be
	 * possible to receive pending data from the socket buffer.
	 */
	static constexpr unsigned DEAD_MASK = ERROR|HANGUP;
};
