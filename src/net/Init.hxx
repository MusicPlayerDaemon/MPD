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
