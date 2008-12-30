/*
 * Copyright (C) 2003-2008 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "zeroconf-internal.h"
#include "listen.h"
#include "ioops.h"

#include <glib.h>

#include <dns_sd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "bonjour"

static struct ioOps zeroConfIo;

static DNSServiceRef dnsReference;

static int dnsRegisterFdset(fd_set * rfds, fd_set * wfds, fd_set * efds)
{
	int fd;

	if (dnsReference == NULL)
		return -1;

	fd = DNSServiceRefSockFD(dnsReference);
	if (fd == -1)
		return -1;

	FD_SET(fd, rfds);

	return fd;
}

static int dnsRegisterFdconsume(int fdCount, fd_set * rfds, fd_set * wfds,
				fd_set * efds)
{
	int fd;

	if (dnsReference == NULL)
		return -1;

	fd = DNSServiceRefSockFD(dnsReference);
	if (fd == -1)
		return -1;

	if (FD_ISSET(fd, rfds)) {
		FD_CLR(fd, rfds);

		DNSServiceProcessResult(dnsReference);

		return fdCount - 1;
	}

	return fdCount;
}

static void dnsRegisterCallback(DNSServiceRef sdRef, DNSServiceFlags flags,
				DNSServiceErrorType errorCode, const char *name,
				const char *regtype, const char *domain,
				void *context)
{
	if (errorCode != kDNSServiceErr_NoError) {
		g_warning("Failed to register zeroconf service.");

		DNSServiceRefDeallocate(dnsReference);
		dnsReference = NULL;
		deregisterIO(&zeroConfIo);
	} else {
		g_debug("Registered zeroconf service with name '%s'", name);
	}
}

void init_zeroconf_osx(const char *serviceName)
{
	DNSServiceErrorType error = DNSServiceRegister(&dnsReference,
						       0, 0, serviceName,
						       SERVICE_TYPE, NULL, NULL,
						       htons(boundPort), 0,
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

	zeroConfIo.fdset = dnsRegisterFdset;
	zeroConfIo.consume = dnsRegisterFdconsume;
	registerIO(&zeroConfIo);
}

void bonjour_finish(void)
{
	deregisterIO(&zeroConfIo);
	if (dnsReference != NULL) {
		DNSServiceRefDeallocate(dnsReference);
		dnsReference = NULL;
		g_debug("Deregistered Zeroconf service.");
	}
}
