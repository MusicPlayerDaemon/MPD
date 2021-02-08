/*
 * Copyright 2012-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef ALLOCATED_SOCKET_ADDRESS_HXX
#define ALLOCATED_SOCKET_ADDRESS_HXX

#include "SocketAddress.hxx" // IWYU pragma: export
#include "Features.hxx"

#include <utility>

#include <stdlib.h>

struct sockaddr;
struct StringView;

class AllocatedSocketAddress {
public:
	typedef SocketAddress::size_type size_type;

private:
	struct sockaddr *address = nullptr;
	size_type size = 0;

	AllocatedSocketAddress(struct sockaddr *_address,
			       size_type _size)
		:address(_address), size(_size) {}

public:
	AllocatedSocketAddress() = default;

	explicit AllocatedSocketAddress(SocketAddress src) noexcept {
		*this = src;
	}

	AllocatedSocketAddress(const AllocatedSocketAddress &src) noexcept
		:AllocatedSocketAddress((SocketAddress)src) {}

	AllocatedSocketAddress(AllocatedSocketAddress &&src) noexcept
		:address(src.address), size(src.size) {
		src.address = nullptr;
		src.size = 0;
	}

	~AllocatedSocketAddress() {
		free(address);
	}

	AllocatedSocketAddress &operator=(SocketAddress src) noexcept;

	AllocatedSocketAddress &operator=(const AllocatedSocketAddress &src) noexcept {
		return *this = (SocketAddress)src;
	}

	AllocatedSocketAddress &operator=(AllocatedSocketAddress &&src) noexcept {
		using std::swap;
		swap(address, src.address);
		swap(size, src.size);
		return *this;
	}

	template<typename T>
	[[gnu::pure]]
	bool operator==(T &&other) const noexcept {
		return (SocketAddress)*this == std::forward<T>(other);
	}

	template<typename T>
	[[gnu::pure]]
	bool operator!=(T &&other) const noexcept {
		return !(*this == std::forward<T>(other));
	}

	[[gnu::const]]
	static AllocatedSocketAddress Null() noexcept {
		return AllocatedSocketAddress(nullptr, 0);
	}

	bool IsNull() const noexcept {
		return address == nullptr;
	}

	size_type GetSize() const noexcept {
		return size;
	}

	const struct sockaddr *GetAddress() const noexcept {
		return address;
	}

	operator SocketAddress() const noexcept {
		return SocketAddress(address, size);
	}

	operator const struct sockaddr *() const noexcept {
		return address;
	}

	int GetFamily() const noexcept {
		return address->sa_family;
	}

	/**
	 * Does the object have a well-defined address?  Check !IsNull()
	 * before calling this method.
	 */
	bool IsDefined() const noexcept {
		return GetFamily() != AF_UNSPEC;
	}

	void Clear() noexcept {
		free(address);
		address = nullptr;
		size = 0;
	}

#ifdef HAVE_UN
	/**
	 * @see SocketAddress::GetLocalRaw()
	 */
	[[gnu::pure]]
	StringView GetLocalRaw() const noexcept;

	/**
	 * @see SocketAddress::GetLocalPath()
	 */
	[[gnu::pure]]
	const char *GetLocalPath() const noexcept {
		return ((SocketAddress)*this).GetLocalPath();
	}

	/**
	 * Make this a "local" address (UNIX domain socket).  If the path
	 * begins with a '@', then the rest specifies an "abstract" local
	 * address.
	 */
	void SetLocal(const char *path) noexcept;
#endif

#ifdef HAVE_TCP
	bool IsV6Any() const noexcept {
		return ((SocketAddress)*this).IsV6Any();
	}

	bool IsV4Mapped() const noexcept {
		return ((SocketAddress)*this).IsV4Mapped();
	}

	/**
	 * Extract the port number.  Returns 0 if not applicable.
	 */
	[[gnu::pure]]
	unsigned GetPort() const noexcept {
		return ((SocketAddress)*this).GetPort();
	}

	/**
	 * @return true on success, false if this address cannot have
	 * a port number
	 */
	bool SetPort(unsigned port) noexcept;

	static AllocatedSocketAddress WithPort(SocketAddress src,
					       unsigned port) noexcept {
		AllocatedSocketAddress result(src);
		result.SetPort(port);
		return result;
	}

	AllocatedSocketAddress WithPort(unsigned port) const noexcept {
		return WithPort(*this, port);
	}
#endif

private:
	void SetSize(size_type new_size) noexcept;
};

#endif
