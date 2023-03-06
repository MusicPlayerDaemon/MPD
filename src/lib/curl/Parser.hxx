// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <span>

class CurlResponseParser {
public:
	virtual ~CurlResponseParser() = default;
	virtual void OnData(std::span<const std::byte> data) = 0;
	virtual void OnEnd() = 0;
};
