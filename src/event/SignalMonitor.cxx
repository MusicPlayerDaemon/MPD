/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "SignalMonitor.hxx"

#ifndef WIN32

#include "WakeFD.hxx"
#include "SocketMonitor.hxx"
#include "util/Manual.hxx"
#include "system/FatalError.hxx"

#include <atomic>
#include <algorithm>

class SignalMonitor final : private SocketMonitor {
	WakeFD fd;

public:
	SignalMonitor(EventLoop &_loop)
		:SocketMonitor(_loop) {
		SocketMonitor::Open(fd.Get());
		SocketMonitor::ScheduleRead();
	}

	~SignalMonitor() {
		/* prevent the descriptor to be closed twice */
		SocketMonitor::Steal();
	}

	void WakeUp() {
		fd.Write();
	}

private:
	virtual bool OnSocketReady(unsigned flags) override;
};

/* this should be enough - is it? */
static constexpr unsigned MAX_SIGNAL = 64;

static SignalHandler signal_handlers[MAX_SIGNAL];
static std::atomic_bool signal_pending[MAX_SIGNAL];

static Manual<SignalMonitor> monitor;

static void
SignalCallback(int signo)
{
	assert(signal_handlers[signo] != nullptr);

	if (!signal_pending[signo].exchange(true))
		monitor->WakeUp();
}

void
SignalMonitorInit(EventLoop &loop)
{
	monitor.Construct(loop);
}

static void
x_sigaction(int signum, const struct sigaction &act)
{
	if (sigaction(signum, &act, nullptr) < 0)
		FatalSystemError("sigaction() failed");
}

void
SignalMonitorFinish()
{
	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_DFL;

	for (unsigned i = 0; i < MAX_SIGNAL; ++i) {
		if (signal_handlers[i] != nullptr) {
			x_sigaction(i, sa);
			signal_handlers[i] = nullptr;
		}
	}

	std::fill_n(signal_pending, MAX_SIGNAL, false);

	monitor.Destruct();
}

void
SignalMonitorRegister(int signo, SignalHandler handler)
{
	assert(signal_handlers[signo] == nullptr);
	assert(!signal_pending[signo]);

	signal_handlers[signo] = handler;

	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SignalCallback;
	x_sigaction(signo, sa);
}

bool
SignalMonitor::OnSocketReady(unsigned)
{
	fd.Read();

	for (unsigned i = 0; i < MAX_SIGNAL; ++i)
		if (signal_pending[i].exchange(false))
			signal_handlers[i]();

	return true;
}

#endif
