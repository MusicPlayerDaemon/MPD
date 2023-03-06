// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SOCKET_SIGNAL_MONITOR_HXX
#define MPD_SOCKET_SIGNAL_MONITOR_HXX

class EventLoop;

#ifndef _WIN32

#include "util/BindMethod.hxx"

using SignalHandler = BoundMethod<void() noexcept>;

/**
 * Initialise the signal monitor subsystem.
 *
 * Throws on error.
 */
void
SignalMonitorInit(EventLoop &loop);

/**
 * Deinitialise the signal monitor subsystem.
 */
void
SignalMonitorFinish() noexcept;

/**
 * Register a handler for the specified signal.  The handler will be
 * invoked in a safe context.
 */
void
SignalMonitorRegister(int signo, SignalHandler handler);

#else

static inline void
SignalMonitorInit(EventLoop &)
{
}

static inline void
SignalMonitorFinish() noexcept
{
}

#endif

#endif /* MAIN_NOTIFY_H */
