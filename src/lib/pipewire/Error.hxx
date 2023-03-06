// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <system_error>

struct AvahiClient;

namespace PipeWire {

class ErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "pipewire";
	}

	std::string message(int condition) const override;
};

extern ErrorCategory error_category;

inline std::system_error
MakeError(int error, const char *msg) noexcept
{
	return std::system_error(error, error_category, msg);
}

} // namespace PipeWire
