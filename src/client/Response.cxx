/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "Response.hxx"
#include "Client.hxx"

#include <fmt/format.h>

TagMask
Response::GetTagMask() const noexcept
{
	return GetClient().tag_mask;
}

bool
Response::Write(const void *data, size_t length) noexcept
{
	return client.Write(data, length);
}

bool
Response::Write(const char *data) noexcept
{
	return client.Write(data);
}

bool
Response::VFmt(fmt::string_view format_str, fmt::format_args args) noexcept
{
	fmt::memory_buffer buffer;
#if FMT_VERSION >= 80000
	fmt::vformat_to(std::back_inserter(buffer), format_str, args);
#else
	fmt::vformat_to(buffer, format_str, args);
#endif
	return Write(buffer.data(), buffer.size());
}

bool
Response::WriteBinary(ConstBuffer<void> payload) noexcept
{
	assert(payload.size <= client.binary_limit);

	return
		Fmt("binary: {}\n", payload.size) &&
		Write(payload.data, payload.size) &&
		Write("\n");
}

void
Response::Error(enum ack code, const char *msg) noexcept
{
	Fmt(FMT_STRING("ACK [{}@{}] {{{}}} "),
	    (int)code, list_index, command);

	Write(msg);
	Write("\n");
}

void
Response::VFmtError(enum ack code,
		    fmt::string_view format_str, fmt::format_args args) noexcept
{
	Fmt(FMT_STRING("ACK [{}@{}] {{{}}} "),
	    (int)code, list_index, command);

	VFmt(format_str, args);

	Write("\n");
}
