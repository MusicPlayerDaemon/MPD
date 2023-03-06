// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SignalHandlers.hxx"
#include "Instance.hxx"
#include "event/SignalMonitor.hxx"

#ifndef _WIN32

#include "Log.hxx"
#include "LogInit.hxx"
#include "system/Error.hxx"
#include "util/Domain.hxx"

#include <csignal>

#ifdef __linux__
#include <sys/prctl.h>
#endif

static constexpr Domain signal_handlers_domain("signal_handlers");

static void
HandleShutdownSignal(void *ctx) noexcept
{
	auto &loop = *(EventLoop *)ctx;
	loop.Break();
}

static void
x_sigaction(int signum, const struct sigaction *act)
{
	if (sigaction(signum, act, nullptr) < 0)
		throw MakeErrno("sigaction() failed");
}

static void
handle_reload_event(void *ctx) noexcept
{
	auto &instance = *(Instance *)ctx;

	LogDebug(signal_handlers_domain, "got SIGHUP, reopening log files and flushing caches");
	cycle_log_files();

	instance.FlushCaches();
}

#endif

void
SignalHandlersInit(Instance &instance, bool daemon)
{
	auto &loop = instance.event_loop;

	SignalMonitorInit(loop);

#ifndef _WIN32
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	x_sigaction(SIGPIPE, &sa);

	SignalMonitorRegister(SIGINT, {&loop, HandleShutdownSignal});
	SignalMonitorRegister(SIGTERM, {&loop, HandleShutdownSignal});

	SignalMonitorRegister(SIGHUP, {&instance, handle_reload_event});
#endif

	if (!daemon) {
#ifdef __linux__
		/* if MPD was not daemonized, shut it down when the
		   parent process dies */
		prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
	}
}

void
SignalHandlersFinish() noexcept
{
	SignalMonitorFinish();
}
