// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "IPv4Address.hxx"

#include <cassert>

IPv4Address::IPv4Address(SocketAddress src) noexcept
	:address(src.CastTo<struct sockaddr_in>())
{
	assert(!src.IsNull());
	assert(src.GetFamily() == AF_INET);
}
