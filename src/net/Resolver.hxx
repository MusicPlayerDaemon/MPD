// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class AddressInfoList;

/**
 * Thin wrapper for getaddrinfo() which throws on error and returns a
 * RAII object.
 */
AddressInfoList
Resolve(const char *node, const char *service,
	const struct addrinfo *hints);

/**
 * Resolve the given host name (which may include a port), and fall
 * back to the given default port.
 *
 * This is a wrapper for getaddrinfo() and it does not support local
 * sockets.
 *
 * Throws on error.
 */
AddressInfoList
Resolve(const char *host_and_port, int default_port,
	const struct addrinfo *hints);

AddressInfoList
Resolve(const char *host_port, unsigned default_port, int flags, int socktype);
