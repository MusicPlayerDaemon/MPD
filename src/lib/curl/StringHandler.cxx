// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "StringHandler.hxx"
#include "util/SpanCast.hxx"

void
StringCurlResponseHandler::OnHeaders(unsigned status, Curl::Headers &&headers)
{
	response.status = status;
	response.headers = std::move(headers);
}

void
StringCurlResponseHandler::OnData(std::span<const std::byte> data)
{
	response.body.append(ToStringView(data));
}

void
StringCurlResponseHandler::OnEnd()
{
}

void
StringCurlResponseHandler::OnError(std::exception_ptr e) noexcept
{
	error = std::move(e);
}
