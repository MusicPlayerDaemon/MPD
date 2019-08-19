/*
 * Copyright 2012-2019 Max Kellermann <max.kellermann@gmail.com>
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
#include "Features.hxx"
#include "util/Compiler.h"

#include <assert.h>

/**
 * An OO wrapper for struct sockaddr_storage.
 */
class StaticSocketAddress {
	friend class SocketDescriptor;

public:
	typedef SocketAddress::size_type size_type;

private:
	size_type size;
	struct sockaddr_storage address;

public:
	StaticSocketAddress() = default;

	StaticSocketAddress &operator=(SocketAddress other) noexcept;

	operator SocketAddress() const noexcept {
		return SocketAddress(reinterpret_cast<const struct sockaddr *>(&address),
				     size);
	}

	operator struct sockaddr *() noexcept {
		return reinterpret_cast<struct sockaddr *>(&address);
	}

	operator const struct sockaddr *() const noexcept {
		return reinterpret_cast<const struct sockaddr *>(&address);
	}

	constexpr size_type GetCapacity() const noexcept {
		return sizeof(address);
	}

	size_type GetSize() const noexcept {
		return size;
	}

	void SetSize(size_type _size) noexcept {
		assert(_size > 0);
		assert(size_t(_size) <= sizeof(address));

		size = _size;
	}

	/**
	 * Set the size to the maximum value for this class.
	 */
	void SetMaxSize() {
		SetSize(GetCapacity());
	}

	int GetFamily() const noexcept {
		return address.ss_family;
	}

	bool IsDefined() const noexcept {
		return GetFamily() != AF_UNSPEC;
	}

	void Clear() noexcept {
		size = sizeof(address.ss_family);
		address.ss_family = AF_UNSPEC;
	}

#ifdef HAVE_UN
	/**
	 * @see SocketAddress::GetLocalRaw()
	 */
	gcc_pure
	StringView GetLocalRaw() const noexcept;
#endif

#ifdef HAVE_TCP
	/**
	 * Extract the port number.  Returns 0 if not applicable.
	 */
	gcc_pure
	unsigned GetPort() const noexcept {
		return ((SocketAddress)*this).GetPort();
	}

	/**
	 * @return true on success, false if this address cannot have
	 * a port number
	 */
	bool SetPort(unsigned port) noexcept;
#endif

	gcc_pure
	bool operator==(SocketAddress other) const noexcept {
		return (SocketAddress)*this == other;
	}

	bool operator!=(SocketAddress other) const noexcept {
		return !(*this == other);
	}
};

#endif
