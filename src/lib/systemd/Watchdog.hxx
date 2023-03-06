// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef SYSTEMD_WATCHDOG_HXX
#define SYSTEMD_WATCHDOG_HXX

#include "event/FineTimerEvent.hxx"

namespace Systemd {

/**
 * This class implements the systemd watchdog protocol; see
 * systemd.service(5) and sd_watchdog_enabled(3).  If the watchdog is
 * not enabled, this class does nothing.
 */
class Watchdog {
	FineTimerEvent timer;

	Event::Duration interval;

public:
	explicit Watchdog(EventLoop &_loop) noexcept;

private:
	void OnTimer() noexcept;
};

} // namespace Systemd

#endif
