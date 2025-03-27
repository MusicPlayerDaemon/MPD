// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#ifdef _WIN32
#include "SocketError.hxx"
#include <winsock2.h>
#endif

class ScopeNetInit {
#ifdef _WIN32
public:
	ScopeNetInit() {
		WSADATA sockinfo;
		int retval = WSAStartup(MAKEWORD(2, 2), &sockinfo);
		if (retval != 0)
			throw MakeSocketError(retval, "WSAStartup() failed");
	}

	~ScopeNetInit() noexcept {
		WSACleanup();
	}
#else
public:
	ScopeNetInit() {}
#endif
};
