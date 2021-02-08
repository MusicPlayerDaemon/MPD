/*
 * Copyright 2016-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef NET_ADDRESS_INFO_HXX
#define NET_ADDRESS_INFO_HXX

#include "SocketAddress.hxx"

#include <utility>

#ifdef _WIN32
#include <ws2tcpip.h> // IWYU pragma: export
#else
#include <netdb.h> // IWYU pragma: export
#endif

constexpr struct addrinfo
MakeAddrInfo(int flags, int family, int socktype, int protocol=0) noexcept
{
	struct addrinfo ai{};
	ai.ai_flags = flags;
	ai.ai_family = family;
	ai.ai_socktype = socktype;
	ai.ai_protocol = protocol;
	return ai;
}

class AddressInfo : addrinfo {
	/* this class cannot be instantiated, it can only be cast from
	   a struct addrinfo pointer */
	AddressInfo() = delete;
	~AddressInfo() = delete;

public:
	constexpr int GetFamily() const {
		return ai_family;
	}

	constexpr int GetType() const {
		return ai_socktype;
	}

	constexpr int GetProtocol() const {
		return ai_protocol;
	}

	constexpr operator SocketAddress() const {
		return {ai_addr, (SocketAddress::size_type)ai_addrlen};
	}
};

class AddressInfoList {
	struct addrinfo *value = nullptr;

public:
	AddressInfoList() = default;
	explicit AddressInfoList(struct addrinfo *_value):value(_value) {}

	AddressInfoList(AddressInfoList &&src)
		:value(std::exchange(src.value, nullptr)) {}

	~AddressInfoList() {
		freeaddrinfo(value);
	}

	AddressInfoList &operator=(AddressInfoList &&src) {
		std::swap(value, src.value);
		return *this;
	}

	bool empty() const {
		return value == nullptr;
	}

	const AddressInfo &front() const {
		return *(const AddressInfo *)value;
	}

	/**
	 * Pick the best address from the list, e.g. prefer IPv6 over
	 * IPv4 (if both are available).  We do this because binding
	 * to an IPv6 wildcard address also allows accepting IPv4
	 * connections.
	 */
	[[gnu::pure]]
	const AddressInfo &GetBest() const;

	class const_iterator {
		struct addrinfo *cursor;

	public:
		explicit constexpr const_iterator(struct addrinfo *_cursor)
			:cursor(_cursor) {}

		constexpr bool operator==(const_iterator other) const {
			return cursor == other.cursor;
		}

		constexpr bool operator!=(const_iterator other) const {
			return cursor != other.cursor;
		}

		const_iterator &operator++() {
			cursor = cursor->ai_next;
			return *this;
		}

		constexpr const AddressInfo &operator*() const {
			return *(const AddressInfo *)cursor;
		}

		constexpr const AddressInfo *operator->() const {
			return (const AddressInfo *)cursor;
		}
	};

	const_iterator begin() const {
		return const_iterator(value);
	}

	const_iterator end() const {
		return const_iterator(nullptr);
	}
};

#endif
