/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "ZeroconfBonjour.hxx"
#include "ZeroconfInternal.hxx"
#include "Listen.hxx"
#include "event/SocketMonitor.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "Compiler.h"

#include <dns_sd.h>

#include <arpa/inet.h>

static constexpr Domain bonjour_domain("bonjour");

class BonjourMonitor final : public SocketMonitor {
	DNSServiceRef service_ref;

public:
	BonjourMonitor(EventLoop &_loop, DNSServiceRef _service_ref)
		:SocketMonitor(DNSServiceRefSockFD(_service_ref), _loop),
		 service_ref(_service_ref) {
		ScheduleRead();
	}

	~BonjourMonitor() {
		DNSServiceRefDeallocate(service_ref);
	}

protected:
	virtual bool OnSocketReady(gcc_unused unsigned flags) override {
		DNSServiceProcessResult(service_ref);
		return false;
	}
};

static BonjourMonitor *bonjour_monitor;

static void
dnsRegisterCallback(gcc_unused DNSServiceRef sdRef,
		    gcc_unused DNSServiceFlags flags,
		    DNSServiceErrorType errorCode, const char *name,
		    gcc_unused const char *regtype,
		    gcc_unused const char *domain,
		    gcc_unused void *context)
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
BonjourInit(EventLoop &loop, const char *service_name)
{
	DNSServiceRef dnsReference;
	DNSServiceErrorType error = DNSServiceRegister(&dnsReference,
						       0, 0, service_name,
						       SERVICE_TYPE, nullptr, nullptr,
						       htons(listen_port), 0,
						       nullptr,
						       dnsRegisterCallback,
						       nullptr);

	if (error != kDNSServiceErr_NoError) {
		LogError(bonjour_domain,
			 "Failed to register zeroconf service");

		if (dnsReference) {
			DNSServiceRefDeallocate(dnsReference);
			dnsReference = nullptr;
		}
		return;
	}

	bonjour_monitor = new BonjourMonitor(loop, dnsReference);
}

void
BonjourDeinit()
{
	delete bonjour_monitor;
}
