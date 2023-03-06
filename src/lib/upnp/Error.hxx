// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <upnptools.h>

#include <system_error>

namespace Upnp {

class ErrorCategory final : public std::error_category {
public:
	const char *name() const noexcept override {
		return "libupnp";
	}

	std::string message(int condition) const override {
		return UpnpGetErrorMessage(condition);
	}
};

inline ErrorCategory error_category;

inline std::system_error
MakeError(int error, const char *msg) noexcept
{
	return std::system_error(error, error_category, msg);
}

} // namespace Upnp
