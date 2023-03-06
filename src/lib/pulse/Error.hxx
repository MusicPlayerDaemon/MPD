// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <system_error>

struct pa_context;

namespace Pulse {

class ErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "pulse";
	}

	std::string message(int condition) const override;
};

extern ErrorCategory error_category;

[[nodiscard]] [[gnu::pure]]
inline std::system_error
MakeError(int error, const char *msg) noexcept
{
	return std::system_error(error, error_category, msg);
}

[[nodiscard]] [[gnu::pure]]
std::system_error
MakeError(pa_context *context, const char *msg) noexcept;

} // namespace Pulse
