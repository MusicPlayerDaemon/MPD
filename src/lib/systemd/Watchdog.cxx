// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Watchdog.hxx"

#include <systemd/sd-daemon.h>

namespace Systemd {

Watchdog::Watchdog(EventLoop &_loop) noexcept
	:timer(_loop, BIND_THIS_METHOD(OnTimer))
{
	uint64_t usec;
	if (sd_watchdog_enabled(true, &usec) <= 0)
		return;

	interval = std::chrono::microseconds(usec) / 2;
	timer.Schedule(interval);
}

void
Watchdog::OnTimer() noexcept
{
	sd_notify(0, "WATCHDOG=1");
	timer.Schedule(interval);
}

} // namespace Systemd
