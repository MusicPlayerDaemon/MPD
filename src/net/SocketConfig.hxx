// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "AllocatedSocketAddress.hxx"
#include "Features.hxx"

#include <cstdint>
#include <utility>

#if defined(HAVE_TCP) && defined(__linux__)
#include <string>
#endif

class UniqueSocketDescriptor;

struct SocketConfig {
	AllocatedSocketAddress bind_address;

#ifdef HAVE_MULTICAST
	AllocatedSocketAddress multicast_group;
#endif

#if defined(HAVE_TCP) && defined(__linux__)
	/**
	 * If non-empty, sets SO_BINDTODEVICE.
	 */
	std::string interface;
#endif

	/**
	 * If non-zero, calls listen().  Value is the backlog.
	 */
	unsigned listen = 0;

#ifdef HAVE_TCP
	/**
	 * If non-zero, sets TCP_DEFER_ACCEPT.  Value is a number of
	 * seconds.
	 */
	unsigned tcp_defer_accept = 0;

	/**
	 * If non-zero, sets TCP_USER_TIMEOUT.  Value is a number of
	 * milliseconds.
	 */
	unsigned tcp_user_timeout = 0;
#endif // HAVE_TCP

#ifdef HAVE_UN
	/**
	 * If non-zero, sets the socket's file mode (overriding the
	 * umask).
	 */
	uint16_t mode = 0;
#endif

#ifdef HAVE_TCP
#ifdef __linux__
	/**
	 * Enable Multi-Path TCP?
	 */
	bool mptcp = false;
#endif

	bool reuse_port = false;

	bool free_bind = false;
#endif

#ifdef HAVE_UN
	bool pass_cred = false;
#endif

#ifdef HAVE_TCP
	/**
	 * If true, then disable Nagle's algorithm.
	 *
	 * @see TCP_NODELAY
	 */
	bool tcp_no_delay = false;

	/**
	 * @see SO_KEEPALIVE
	 */
	bool keepalive = false;
#endif

#ifdef HAVE_IPV6
	/**
	 * @see IPV6_V6ONLY
	 */
	bool v6only = false;
#endif

	/**
	 * Apply fixups after configuration:
	 *
	 * - if bind_address is IPv6-wildcard, but multicast_group is
	 *   IPv4, then change bind_address to IPv4-wildcard
	 */
	void Fixup() noexcept;

	/**
	 * Create a listening socket.
	 *
	 * Throws exception on error.
	 */
	UniqueSocketDescriptor Create(int type) const;
};
