// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Error.hxx"

#include <pulse/context.h>
#include <pulse/error.h>

namespace Pulse {

std::string
ErrorCategory::message(int condition) const
{
	return pa_strerror(condition);
}

ErrorCategory error_category;

std::system_error
MakeError(pa_context *context, const char *msg) noexcept
{
	return MakeError(pa_context_errno(context), msg);
}

} // namespace Pulse
