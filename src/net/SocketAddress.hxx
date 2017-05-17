/*
 * Copyright (C) 2012-2017 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef SOCKET_ADDRESS_HXX
#define SOCKET_ADDRESS_HXX

#include "Features.hxx"
#include "Compiler.h"

#include <cstddef>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

/**
 * An OO wrapper for struct sockaddr.
 */
class SocketAddress {
public:
#ifdef WIN32
	typedef int size_type;
#else
	typedef socklen_t size_type;
#endif

private:
	const struct sockaddr *address;
	size_type size;

public:
	SocketAddress() = default;

	constexpr SocketAddress(std::nullptr_t) noexcept
		:address(nullptr), size(0) {}

	constexpr SocketAddress(const struct sockaddr *_address,
				size_type _size) noexcept
		:address(_address), size(_size) {}

	static constexpr SocketAddress Null() noexcept {
		return nullptr;
	}

	constexpr bool IsNull() const noexcept {
		return address == nullptr;
	}

	const struct sockaddr *GetAddress() const noexcept {
		return address;
	}

	constexpr size_type GetSize() const noexcept {
		return size;
	}

	constexpr int GetFamily() const noexcept {
		return address->sa_family;
	}

	/**
	 * Does the object have a well-defined address?  Check !IsNull()
	 * before calling this method.
	 */
	bool IsDefined() const noexcept {
		return GetFamily() != AF_UNSPEC;
	}

#ifdef HAVE_TCP
	/**
	 * Extract the port number.  Returns 0 if not applicable.
	 */
	gcc_pure
	unsigned GetPort() const noexcept;
#endif

	gcc_pure
	bool operator==(const SocketAddress other) const noexcept;

	bool operator!=(const SocketAddress other) const noexcept {
		return !(*this == other);
	}
};

#endif
