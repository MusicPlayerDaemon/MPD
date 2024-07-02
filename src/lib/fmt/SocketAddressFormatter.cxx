// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketAddressFormatter.hxx"
#include "net/ToString.hxx"

auto
fmt::formatter<SocketAddress>::format(SocketAddress address, format_context &ctx)
  -> format_context::iterator
{
	return formatter<string_view>::format(ToString(address), ctx);
}
