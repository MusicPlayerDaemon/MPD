// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef TEXT_FILE_HXX
#define TEXT_FILE_HXX

#include <cstring>

template<typename B>
char *
ReadBufferedLine(B &buffer)
{
	auto r = buffer.Read();
	char *newline = reinterpret_cast<char*>(std::memchr(r.data(), '\n', r.size()));
	if (newline == nullptr)
		return nullptr;

	buffer.Consume(newline + 1 - r.data());

	if (newline > r.data() && newline[-1] == '\r')
		--newline;
	*newline = 0;
	return r.data();
}

#endif
