// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PULSE_LOG_ERROR_HXX
#define MPD_PULSE_LOG_ERROR_HXX

struct pa_context;

void
LogPulseError(pa_context *context, const char *prefix) noexcept;

#endif
