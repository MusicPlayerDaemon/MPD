// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_LOG_BACKEND_HXX
#define MPD_LOG_BACKEND_HXX

#include "LogLevel.hxx"
#include "Log.hxx"

void
SetLogThreshold(LogLevel _threshold) noexcept;

void
EnableLogTimestamp(LogTimestamp _log_time_stamp) noexcept;

void
LogInitSysLog() noexcept;

void
LogFinishSysLog() noexcept;

#endif /* LOG_H */
