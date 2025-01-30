// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
