// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "OutputStream.hxx"
#include "util/SpanCast.hxx"

#include <string>

class StringOutputStream final : public OutputStream {
	std::string value;

public:
	const std::string &GetValue() const & noexcept {
		return value;
	}

	std::string &&GetValue() && noexcept {
		return std::move(value);
	}

	/* virtual methods from class OutputStream */
	void Write(std::span<const std::byte> src) override {
		value.append(ToStringView(src));
	}
};
