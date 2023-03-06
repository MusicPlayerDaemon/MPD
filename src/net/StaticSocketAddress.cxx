// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "StaticSocketAddress.hxx"
#include "IPv4Address.hxx"
#include "IPv6Address.hxx"

#include <algorithm>

#include <string.h>

StaticSocketAddress &
StaticSocketAddress::operator=(SocketAddress other) noexcept
{
	size = std::min(other.GetSize(), GetCapacity());
	memcpy(&address, other.GetAddress(), size);
	return *this;
}

#ifdef HAVE_UN

std::string_view
StaticSocketAddress::GetLocalRaw() const noexcept
{
	return SocketAddress(*this).GetLocalRaw();
}

#endif

#ifdef HAVE_TCP

bool
StaticSocketAddress::SetPort(unsigned port) noexcept
{
	switch (GetFamily()) {
	case AF_INET:
		{
			auto &a = *(IPv4Address *)(void *)&address;
			a.SetPort(port);
			return true;
		}

	case AF_INET6:
		{
			auto &a = *(IPv6Address *)(void *)&address;
			a.SetPort(port);
			return true;
		}
	}

	return false;
}

#endif
