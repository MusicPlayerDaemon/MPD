// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SocketAddress.hxx"

#include <iterator>
#include <utility>

#ifdef _WIN32
#include <ws2tcpip.h> // IWYU pragma: export
#else
#include <netdb.h> // IWYU pragma: export
#endif

class AddressInfo : addrinfo {
	/* this class cannot be instantiated, it can only be cast from
	   a struct addrinfo pointer */
	AddressInfo() = delete;
	~AddressInfo() = delete;

public:
	constexpr int GetFamily() const noexcept {
		return ai_family;
	}

	constexpr int GetType() const noexcept {
		return ai_socktype;
	}

	constexpr int GetProtocol() const noexcept {
		return ai_protocol;
	}

	constexpr bool IsInet() const noexcept {
		return ai_family == AF_INET || ai_family == AF_INET6;
	}

	constexpr bool IsTCP() const noexcept {
		return IsInet() && GetType() == SOCK_STREAM;
	}

	constexpr operator SocketAddress() const noexcept {
		return {ai_addr, (SocketAddress::size_type)ai_addrlen};
	}

	/**
	 * Cast a #addrinfo reference to an #AddressInfo reference.
	 */
	static constexpr const AddressInfo &Cast(const struct addrinfo &ai) noexcept {
		return static_cast<const AddressInfo &>(ai);
	}
};

class AddressInfoList {
	struct addrinfo *value = nullptr;

public:
	AddressInfoList() = default;
	explicit AddressInfoList(struct addrinfo *_value) noexcept
		:value(_value) {}

	AddressInfoList(AddressInfoList &&src) noexcept
		:value(std::exchange(src.value, nullptr)) {}

	~AddressInfoList() noexcept {
		freeaddrinfo(value);
	}

	AddressInfoList &operator=(AddressInfoList &&src) noexcept {
		std::swap(value, src.value);
		return *this;
	}

	bool empty() const noexcept {
		return value == nullptr;
	}

	const AddressInfo &front() const noexcept {
		return *(const AddressInfo *)value;
	}

	/**
	 * Pick the best address from the list, e.g. prefer IPv6 over
	 * IPv4 (if both are available).  We do this because binding
	 * to an IPv6 wildcard address also allows accepting IPv4
	 * connections.
	 */
	[[gnu::pure]]
	const AddressInfo &GetBest() const noexcept;

	class const_iterator {
		struct addrinfo *cursor;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = AddressInfo;
		using difference_type = std::ptrdiff_t;
		using pointer = const value_type *;
		using reference = const value_type &;

		explicit constexpr const_iterator(struct addrinfo *_cursor) noexcept
			:cursor(_cursor) {}

		constexpr bool operator==(const_iterator other) const noexcept {
			return cursor == other.cursor;
		}

		const_iterator &operator++() noexcept {
			cursor = cursor->ai_next;
			return *this;
		}

		constexpr const AddressInfo &operator*() const noexcept {
			return *(const AddressInfo *)cursor;
		}

		constexpr const AddressInfo *operator->() const noexcept {
			return (const AddressInfo *)cursor;
		}
	};

	const_iterator begin() const noexcept {
		return const_iterator(value);
	}

	const_iterator end() const noexcept {
		return const_iterator(nullptr);
	}
};
