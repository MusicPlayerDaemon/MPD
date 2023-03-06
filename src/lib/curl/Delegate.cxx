// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Delegate.hxx"
#include "Parser.hxx"

#include <cassert>
#include <utility>

void
DelegateCurlResponseHandler::OnHeaders(unsigned status, Curl::Headers &&headers)
{
	parser = MakeParser(status, std::move(headers));
	assert(parser);
}

void
DelegateCurlResponseHandler::OnData(std::span<const std::byte> data)
{
	parser->OnData(data);
}

void
DelegateCurlResponseHandler::OnEnd()
{
	parser->OnEnd();
	FinishParser(std::move(parser));
}
