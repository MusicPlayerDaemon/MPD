// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "IPv4Address.hxx"
#include "util/CharUtil.hxx"

#include <concepts>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

template<std::unsigned_integral T>
[[nodiscard]]
constexpr std::pair<T, const char *>
ParseDecimalU(const char *p, const char *const end)
{
	static_assert(std::numeric_limits<T>::max() < std::numeric_limits<uint_fast32_t>::max());
	uint_fast32_t value = 0;

	if (p == end || !IsDigitASCII(*p))
		throw std::invalid_argument{"Number expected"};

	char ch = *p++;

	while (true) {
		value = (value * 10) + (ch - '0');
		if (value > std::numeric_limits<T>::max())
			throw std::overflow_error{"Value too large"};

		if (p == end)
			break;

		ch = *p;
		if (!IsDigitASCII(ch))
			break;

		p++;
	}

	return {value, p};
}

[[nodiscard]]
constexpr IPv4Address
ParseIPv4Address(const char *s, const char *const end)
{
	auto [a, t] = ParseDecimalU<uint8_t>(s, end);
	if (*t++ != '.')
		throw std::invalid_argument{"Dot expected"};

	auto [b, u] = ParseDecimalU<uint8_t>(t, end);
	if (*u++ != '.')
		throw std::invalid_argument{"Dot expected"};

	auto [c, v] = ParseDecimalU<uint8_t>(u, end);
	if (*v++ != '.')
		throw std::invalid_argument{"Dot expected"};

	const auto [d, w] = ParseDecimalU<uint8_t>(v, end);

	uint16_t port = 0;
	if (*w == ':') {
		const auto [_port, x] = ParseDecimalU<uint16_t>(w + 1, end);
		if (*x != '\0')
			throw std::invalid_argument{"Garbage after port number"};

		port = _port;
	} else if (*w != '\0')
		throw std::invalid_argument{"Garbage after address"};

	return IPv4Address{a, b, c, d, port};
}

[[nodiscard]] [[gnu::pure]]
constexpr IPv4Address
operator ""_ipv4(const char *data, std::size_t size)
{
	return ParseIPv4Address(data, data + size);
}
