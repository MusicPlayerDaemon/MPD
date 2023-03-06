// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <system_error>

namespace Alsa {

class ErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "libasound";
	}

	std::string message(int condition) const override;
};

extern ErrorCategory error_category;

inline std::system_error
MakeError(int error, const char *msg) noexcept
{
	return std::system_error(error, error_category, msg);
}

} // namespace Alsa
