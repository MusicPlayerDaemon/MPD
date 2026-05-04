// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "StringHandler.hxx"
#include "util/SpanCast.hxx"

namespace Curl {

void
StringResponseHandler::OnHeaders(unsigned status, Curl::Headers &&headers)
{
	response.status = status;
	response.headers = std::move(headers);
}

void
StringResponseHandler::OnData(std::span<const std::byte> data)
{
	response.body.append(ToStringView(data));
}

void
StringResponseHandler::OnEnd()
{
}

void
StringResponseHandler::OnError(std::exception_ptr e) noexcept
{
	error = std::move(e);
}

} // namespace Curl
