// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ShutdownHandler.hxx"

#ifndef _WIN32
#include "event/SignalMonitor.hxx"
#include "event/Loop.hxx"

#include <signal.h>

static void
HandleShutdownSignal(void *ctx) noexcept
{
	auto &loop = *(EventLoop *)ctx;
	loop.Break();
}

ShutdownHandler::ShutdownHandler(EventLoop &loop)
{
	SignalMonitorInit(loop);

	SignalMonitorRegister(SIGINT, {&loop, HandleShutdownSignal});
	SignalMonitorRegister(SIGTERM, {&loop, HandleShutdownSignal});
}

ShutdownHandler::~ShutdownHandler()
{
	SignalMonitorFinish();
}

#endif
