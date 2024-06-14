// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <zlib.h>

#include <system_error>

class ZlibErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "zlib";
	}

	std::string message(int condition) const override {
		return zError(condition);
	}
};

inline ZlibErrorCategory zlib_error_category;

inline std::system_error
MakeZlibError(int code, const char *msg) noexcept
{
	return std::system_error(code, zlib_error_category, msg);
}
