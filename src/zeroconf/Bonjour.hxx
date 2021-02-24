/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_ZEROCONF_BONJOUR_HXX
#define MPD_ZEROCONF_BONJOUR_HXX

#include "event/SocketEvent.hxx"

#include <dns_sd.h>

#include <memory>

class EventLoop;

class BonjourHelper final {
	const DNSServiceRef service_ref;

	SocketEvent socket_event;

public:
	BonjourHelper(EventLoop &_loop, const char *name,
		      const char *service_name, unsigned port);

	~BonjourHelper() noexcept {
		DNSServiceRefDeallocate(service_ref);
	}

	BonjourHelper(const BonjourHelper &) = delete;
	BonjourHelper &operator=(const BonjourHelper &) = delete;

private:
	void Cancel() noexcept {
		socket_event.Cancel();
	}

	static void Callback(DNSServiceRef sdRef, DNSServiceFlags flags,
			     DNSServiceErrorType errorCode, const char *name,
			     const char *regtype,
			     const char *domain,
			     void *context) noexcept;

	/* virtual methods from class SocketMonitor */
	void OnSocketReady([[maybe_unused]] unsigned flags) noexcept {
		DNSServiceProcessResult(service_ref);
	}
};

/**
 * Throws on error.
 */
std::unique_ptr<BonjourHelper>
BonjourInit(EventLoop &loop, const char *name,
	    const char *service_type, unsigned port);

#endif
