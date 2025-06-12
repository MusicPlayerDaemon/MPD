// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <system_error>

class AddressInfoList;

class ResolverErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "gai";
	}

	std::string message(int condition) const override;
};

extern ResolverErrorCategory resolver_error_category;

/**
 * Thin wrapper for getaddrinfo() which throws on error and returns a
 * RAII object.
 *
 * getaddrinfo() errors are thrown as std::system_error with
 * #resolver_error_category.
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
 * Throws on error.  Resolver errors are thrown as std::system_error
 * with #resolver_error_category.
 */
AddressInfoList
Resolve(const char *host_and_port, int default_port,
	const struct addrinfo *hints);

AddressInfoList
Resolve(const char *host_port, unsigned default_port, int flags, int socktype);
