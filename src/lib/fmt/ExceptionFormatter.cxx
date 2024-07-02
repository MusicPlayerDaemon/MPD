// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "ExceptionFormatter.hxx"
#include "util/Exception.hxx"

auto
fmt::formatter<std::exception_ptr>::format(std::exception_ptr e, format_context &ctx) const
 -> format_context::iterator
{
	return formatter<string_view>::format(GetFullMessage(e), ctx);
}
