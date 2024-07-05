// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketAddressFormatter.hxx"
#include "net/FormatAddress.hxx"

auto
fmt::formatter<SocketAddress>::format(SocketAddress address, format_context &ctx) const
  -> format_context::iterator
{
	char buffer[256];
	std::string_view s;

	if (ToString(std::span{buffer}, address))
		s = buffer;
	else
		s = "?";

	return formatter<string_view>::format(s, ctx);
}
