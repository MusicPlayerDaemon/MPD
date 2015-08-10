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

#ifndef ALLOCATED_SOCKET_ADDRESS_HPP
#define ALLOCATED_SOCKET_ADDRESS_HPP

#include "SocketAddress.hxx"
#include "Features.hxx"
#include "Compiler.h"

#include <algorithm>

#include <stdlib.h>

struct sockaddr;
class Error;

class AllocatedSocketAddress {
public:
	typedef SocketAddress::size_type size_type;

private:
	struct sockaddr *address;
	size_type size;

	AllocatedSocketAddress(struct sockaddr *_address,
			       size_type _size)
		:address(_address), size(_size) {}

public:
	AllocatedSocketAddress():address(nullptr), size(0) {}

	explicit AllocatedSocketAddress(SocketAddress src)
		:address(nullptr), size(0) {
		*this = src;
	}

	AllocatedSocketAddress(const AllocatedSocketAddress &) = delete;

	AllocatedSocketAddress(AllocatedSocketAddress &&src)
		:address(src.address), size(src.size) {
		src.address = nullptr;
		src.size = 0;
	}

	~AllocatedSocketAddress() {
		free(address);
	}

	AllocatedSocketAddress &operator=(SocketAddress src);

	AllocatedSocketAddress &operator=(const AllocatedSocketAddress &) = delete;

	AllocatedSocketAddress &operator=(AllocatedSocketAddress &&src) {
		std::swap(address, src.address);
		std::swap(size, src.size);
		return *this;
	}

	gcc_pure
	bool operator==(SocketAddress other) const {
		return (SocketAddress)*this == other;
	}

	bool operator!=(SocketAddress &other) const {
		return !(*this == other);
	}

	gcc_const
	static AllocatedSocketAddress Null() {
		return AllocatedSocketAddress(nullptr, 0);
	}

	bool IsNull() const {
		return address == nullptr;
	}

	size_type GetSize() const {
		return size;
	}

	const struct sockaddr *GetAddress() const {
		return address;
	}

	operator SocketAddress() const {
		return SocketAddress(address, size);
	}

	operator const struct sockaddr *() const {
		return address;
	}

	int GetFamily() const {
		return address->sa_family;
	}

	/**
	 * Does the object have a well-defined address?  Check !IsNull()
	 * before calling this method.
	 */
	bool IsDefined() const {
		return GetFamily() != AF_UNSPEC;
	}

	void Clear() {
		free(address);
		address = nullptr;
		size = 0;
	}

#ifdef HAVE_UN
	/**
	 * Make this a "local" address (UNIX domain socket).  If the path
	 * begins with a '@', then the rest specifies an "abstract" local
	 * address.
	 */
	void SetLocal(const char *path);
#endif

private:
	void SetSize(size_type new_size);
};

#endif
