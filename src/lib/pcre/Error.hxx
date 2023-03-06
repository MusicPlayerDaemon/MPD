// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <system_error>

namespace Pcre {

class ErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "pcre2";
	}

	std::string message(int condition) const override;
};

extern ErrorCategory error_category;

inline std::system_error
MakeError(int error, const char *msg) noexcept
{
	return std::system_error(error, error_category, msg);
}

} // namespace Pcre
