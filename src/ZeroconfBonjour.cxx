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
#include "ZeroconfBonjour.hxx"
#include "ZeroconfInternal.hxx"
#include "Listen.hxx"
#include "event/SocketMonitor.hxx"
#include "gcc.h"

#include <glib.h>

#include <dns_sd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "bonjour"

class BonjourMonitor final : public SocketMonitor {
	DNSServiceRef service_ref;

public:
	BonjourMonitor(EventLoop &_loop, DNSServiceRef _service_ref)
		:SocketMonitor(DNSServiceRefSockFD(_service_ref), _loop),
		 service_ref(_service_ref) {
		ScheduleRead();
	}

	~BonjourMonitor() {
		Steal();
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
dnsRegisterCallback(G_GNUC_UNUSED DNSServiceRef sdRef,
		    G_GNUC_UNUSED DNSServiceFlags flags,
		    DNSServiceErrorType errorCode, const char *name,
		    G_GNUC_UNUSED const char *regtype,
		    G_GNUC_UNUSED const char *domain,
		    G_GNUC_UNUSED void *context)
{
	if (errorCode != kDNSServiceErr_NoError) {
		g_warning("Failed to register zeroconf service.");

		bonjour_monitor->Cancel();
	} else {
		g_debug("Registered zeroconf service with name '%s'", name);
	}
}

void
BonjourInit(EventLoop &loop, const char *service_name)
{
	DNSServiceRef dnsReference;
	DNSServiceErrorType error = DNSServiceRegister(&dnsReference,
						       0, 0, service_name,
						       SERVICE_TYPE, NULL, NULL,
						       g_htons(listen_port), 0,
						       NULL,
						       dnsRegisterCallback,
						       NULL);

	if (error != kDNSServiceErr_NoError) {
		g_warning("Failed to register zeroconf service.");

		if (dnsReference) {
			DNSServiceRefDeallocate(dnsReference);
			dnsReference = NULL;
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
