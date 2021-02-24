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

#include "Bonjour.hxx"
#include "Internal.hxx"
#include "event/SocketEvent.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "util/Compiler.h"

#include <dns_sd.h>

#include <arpa/inet.h>

static constexpr Domain bonjour_domain("bonjour");

class BonjourHelper final {
	DNSServiceRef service_ref;

	SocketEvent socket_event;

public:
	BonjourHelper(EventLoop &_loop, DNSServiceRef _service_ref)
		:service_ref(_service_ref),
		 socket_event(_loop,
			      BIND_THIS_METHOD(OnSocketReady),
			      SocketDescriptor(DNSServiceRefSockFD(service_ref)))
	{
		socket_event.ScheduleRead();
	}

	~BonjourHelper() {
		DNSServiceRefDeallocate(service_ref);
	}

	BonjourHelper(const BonjourHelper &) = delete;
	BonjourHelper &operator=(const BonjourHelper &) = delete;

	void Cancel() noexcept {
		socket_event.Cancel();
	}

protected:
	/* virtual methods from class SocketMonitor */
	void OnSocketReady([[maybe_unused]] unsigned flags) noexcept {
		DNSServiceProcessResult(service_ref);
	}
};

static BonjourHelper *bonjour_monitor;

static void
dnsRegisterCallback([[maybe_unused]] DNSServiceRef sdRef,
		    [[maybe_unused]] DNSServiceFlags flags,
		    DNSServiceErrorType errorCode, const char *name,
		    [[maybe_unused]] const char *regtype,
		    [[maybe_unused]] const char *domain,
		    [[maybe_unused]] void *context)
{
	if (errorCode != kDNSServiceErr_NoError) {
		LogError(bonjour_domain,
			 "Failed to register zeroconf service");

		bonjour_monitor->Cancel();
	} else {
		FormatDebug(bonjour_domain,
			    "Registered zeroconf service with name '%s'",
			    name);
	}
}

void
BonjourInit(EventLoop &loop, const char *service_name, unsigned port)
{
	DNSServiceRef dnsReference;
	DNSServiceErrorType error = DNSServiceRegister(&dnsReference,
						       0, 0, service_name,
						       SERVICE_TYPE, nullptr, nullptr,
						       htons(port), 0,
						       nullptr,
						       dnsRegisterCallback,
						       nullptr);

	if (error != kDNSServiceErr_NoError) {
		LogError(bonjour_domain,
			 "Failed to register zeroconf service");
		return;
	}

	bonjour_monitor = new BonjourHelper(loop, dnsReference);
}

void
BonjourDeinit()
{
	delete bonjour_monitor;
}
