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

#include <stdexcept>

#include <arpa/inet.h>

static constexpr Domain bonjour_domain("bonjour");

class BonjourHelper final {
	const DNSServiceRef service_ref;

	SocketEvent socket_event;

public:
	BonjourHelper(EventLoop &_loop, const char *name, unsigned port);

	~BonjourHelper() {
		DNSServiceRefDeallocate(service_ref);
	}

	BonjourHelper(const BonjourHelper &) = delete;
	BonjourHelper &operator=(const BonjourHelper &) = delete;

	void Cancel() noexcept {
		socket_event.Cancel();
	}

	static void Callback(DNSServiceRef sdRef, DNSServiceFlags flags,
			     DNSServiceErrorType errorCode, const char *name,
			     const char *regtype,
			     const char *domain,
			     void *context) noexcept;

protected:
	/* virtual methods from class SocketMonitor */
	void OnSocketReady([[maybe_unused]] unsigned flags) noexcept {
		DNSServiceProcessResult(service_ref);
	}
};

/**
 * A wrapper for DNSServiceRegister() which returns the DNSServiceRef
 * and throws on error.
 */
static DNSServiceRef
RegisterBonjour(const char *name, const char *type, unsigned port,
		DNSServiceRegisterReply callback, void *ctx)
{
	DNSServiceRef ref;
	DNSServiceErrorType error = DNSServiceRegister(&ref,
						       0, 0, name, type,
						       nullptr, nullptr,
						       htons(port), 0,
						       nullptr,
						       callback, ctx);

	if (error != kDNSServiceErr_NoError)
		throw std::runtime_error("DNSServiceRegister() failed");

	return ref;
}

BonjourHelper::BonjourHelper(EventLoop &_loop, const char *name, unsigned port)
	:service_ref(RegisterBonjour(name, SERVICE_TYPE, port,
				     Callback, nullptr)),
	 socket_event(_loop,
		      BIND_THIS_METHOD(OnSocketReady),
		      SocketDescriptor(DNSServiceRefSockFD(service_ref)))
{
	socket_event.ScheduleRead();
}

static BonjourHelper *bonjour_monitor;

void
BonjourHelper::Callback([[maybe_unused]] DNSServiceRef sdRef,
			[[maybe_unused]] DNSServiceFlags flags,
			DNSServiceErrorType errorCode, const char *name,
			[[maybe_unused]] const char *regtype,
			[[maybe_unused]] const char *domain,
			[[maybe_unused]] void *context) noexcept
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
	bonjour_monitor = new BonjourHelper(loop, service_name, port);
}

void
BonjourDeinit()
{
	delete bonjour_monitor;
}
