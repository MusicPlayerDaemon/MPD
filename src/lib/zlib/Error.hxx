// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <exception>

class ZlibError final : public std::exception {
	int code;

public:
	explicit ZlibError(int _code) noexcept:code(_code) {}

	int GetCode() const noexcept {
		return code;
	}

	const char *what() const noexcept override;
};
