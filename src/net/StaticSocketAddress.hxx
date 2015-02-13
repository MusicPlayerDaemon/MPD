/*
 * Copyright (C) 2012-2015 Max Kellermann <max@duempel.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef STATIC_SOCKET_ADDRESS_HXX
#define STATIC_SOCKET_ADDRESS_HXX

#include "SocketAddress.hxx"
#include "Compiler.h"

#include <assert.h>
#include <stdint.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

struct ifaddrs;

/**
 * An OO wrapper for struct sockaddr_storage.
 */
class StaticSocketAddress {
	size_t size;
	struct sockaddr_storage address;

public:
	StaticSocketAddress() = default;

	StaticSocketAddress &operator=(SocketAddress other);

	operator SocketAddress() const {
		return SocketAddress(reinterpret_cast<const struct sockaddr *>(&address),
				     size);
	}

#ifdef HAVE_UN
	/**
	 * Make this a "local" address (UNIX domain socket).
	 */
	void SetLocal(const char *path);
#endif

	operator struct sockaddr *() {
		return reinterpret_cast<struct sockaddr *>(&address);
	}

	operator const struct sockaddr *() const {
		return reinterpret_cast<const struct sockaddr *>(&address);
	}

	constexpr size_t GetCapacity() const {
		return sizeof(address);
	}

	size_t GetSize() const {
		return size;
	}

	void SetSize(size_t _size) {
		assert(_size > 0);
		assert(_size <= sizeof(address));

		size = _size;
	}

	int GetFamily() const {
		return address.ss_family;
	}

	bool IsDefined() const {
		return GetFamily() != AF_UNSPEC;
	}

	void Clear() {
		address.ss_family = AF_UNSPEC;
	}

	gcc_pure
	bool operator==(const StaticSocketAddress &other) const;

	bool operator!=(const StaticSocketAddress &other) const {
		return !(*this == other);
	}
};

#endif
