// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstring>

enum class ExtractLineFlush {
	NEVER,
	IF_FULL,
	ALWAYS,
};

template<typename B>
std::span<char>
ExtractLine(B &buffer, ExtractLineFlush flush)
{
	auto r = buffer.Read();
	char *data = reinterpret_cast<char*>(r.data());
	char *newline = reinterpret_cast<char*>(std::memchr(data, '\n', r.size()));
	if (newline == nullptr) {
		if (r.empty())
			return {};

		switch (flush) {
		case ExtractLineFlush::NEVER:
			return {};

		case ExtractLineFlush::IF_FULL:
			if (!buffer.IsFull())
				return {};

			break;

		case ExtractLineFlush::ALWAYS:
			break;
		}

		buffer.Clear();
		return {data, r.size()};
	}

	buffer.Consume(newline + 1 - data);

	while (newline > data && newline[-1] == '\r')
		--newline;

	return std::span{data, static_cast<std::size_t>(newline - data)};
}
