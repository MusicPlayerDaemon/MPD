// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LogError.hxx"
#include "Domain.hxx"
#include "Log.hxx"

#include <pulse/context.h>
#include <pulse/error.h>

void
LogPulseError(pa_context *context, const char *prefix) noexcept
{
	const int e = pa_context_errno(context);
	FmtError(pulse_domain, "{}: {}", prefix, pa_strerror(e));
}
