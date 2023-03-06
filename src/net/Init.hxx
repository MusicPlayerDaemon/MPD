// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef NET_INIT_HXX
#define NET_INIT_HXX

#include "SocketError.hxx"

#ifdef _WIN32
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

#endif
