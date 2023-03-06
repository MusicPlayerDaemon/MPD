// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "ResponseParser.hxx"

void
YajlResponseParser::OnData(std::span<const std::byte> data)
{
	handle.Parse((const unsigned char *)data.data(), data.size());
}

void
YajlResponseParser::OnEnd()
{
	handle.CompleteParse();
}
