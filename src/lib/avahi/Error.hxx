// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <system_error>

struct AvahiClient;

namespace Avahi {

class ErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "avahi-client";
	}

	std::string message(int condition) const override;
};

extern ErrorCategory error_category;

inline std::system_error
MakeError(int error, const char *msg) noexcept
{
	return std::system_error(error, error_category, msg);
}

std::system_error
MakeError(AvahiClient &client, const char *msg) noexcept;

} // namespace Avahi
