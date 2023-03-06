// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <unicode/utypes.h>

#include <system_error>

namespace ICU {

class ErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "icu";
	}

	std::string message(int condition) const override {
		return u_errorName(static_cast<UErrorCode>(condition));
	}
};

inline ErrorCategory error_category;

inline std::system_error
MakeError(UErrorCode code, const char *msg) noexcept
{
	return std::system_error(static_cast<int>(code), error_category, msg);
}

} // namespace Curl
