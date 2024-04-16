// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
	fmt::vformat_to(std::back_inserter(buffer), format_str, args);
	return Write(buffer.data(), buffer.size());
}

bool
Response::WriteBinary(std::span<const std::byte> payload) noexcept
{
	assert(payload.size() <= client.binary_limit);

	return
		Fmt("binary: {}\n", payload.size()) &&
		Write(payload.data(), payload.size()) &&
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
