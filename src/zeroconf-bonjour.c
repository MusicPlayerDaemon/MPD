/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "zeroconf-internal.h"
#include "listen.h"

#include <glib.h>

#include <dns_sd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "bonjour"

static DNSServiceRef dnsReference;
static GIOChannel *bonjour_channel;

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

		bonjour_finish();
	} else {
		g_debug("Registered zeroconf service with name '%s'", name);
	}
}

static gboolean
bonjour_channel_event(G_GNUC_UNUSED GIOChannel *source,
		      G_GNUC_UNUSED GIOCondition condition,
		      G_GNUC_UNUSED gpointer data)
{
	DNSServiceProcessResult(dnsReference);

	return dnsReference != NULL;
}

void init_zeroconf_osx(const char *serviceName)
{
	DNSServiceErrorType error = DNSServiceRegister(&dnsReference,
						       0, 0, serviceName,
						       SERVICE_TYPE, NULL, NULL,
						       htons(listen_port), 0,
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

	bonjour_channel = g_io_channel_unix_new(DNSServiceRefSockFD(dnsReference));
	g_io_add_watch(bonjour_channel, G_IO_IN, bonjour_channel_event, NULL);
}

void bonjour_finish(void)
{
	if (bonjour_channel != NULL) {
		g_io_channel_unref(bonjour_channel);
		bonjour_channel = NULL;
	}

	if (dnsReference != NULL) {
		DNSServiceRefDeallocate(dnsReference);
		dnsReference = NULL;
		g_debug("Deregistered Zeroconf service.");
	}
}
