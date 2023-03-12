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
};
