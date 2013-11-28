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
#include "ZeroconfAvahi.hxx"
#include "AvahiPoll.hxx"
#include "ZeroconfInternal.hxx"
#include "Listen.hxx"
#include "system/FatalError.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/domain.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

static constexpr Domain avahi_domain("avahi");

static char *avahiName;
static bool avahi_running;
static MyAvahiPoll *avahi_poll;
static AvahiClient *avahiClient;
static AvahiEntryGroup *avahiGroup;

static void avahiRegisterService(AvahiClient * c);

/* Callback when the EntryGroup changes state */
static void avahiGroupCallback(AvahiEntryGroup * g,
			       AvahiEntryGroupState state,
			       gcc_unused void *userdata)
{
	char *n;
	assert(g);

	FormatDebug(avahi_domain,
		    "Service group changed to state %d", state);

	switch (state) {
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
		/* The entry group has been established successfully */
		FormatDefault(avahi_domain,
			      "Service '%s' successfully established.",
			      avahiName);
		break;

	case AVAHI_ENTRY_GROUP_COLLISION:
		/* A service name collision happened. Let's pick a new name */
		n = avahi_alternative_service_name(avahiName);
		avahi_free(avahiName);
		avahiName = n;

		FormatDefault(avahi_domain,
			      "Service name collision, renaming service to '%s'",
			      avahiName);

		/* And recreate the services */
		avahiRegisterService(avahi_entry_group_get_client(g));
		break;

	case AVAHI_ENTRY_GROUP_FAILURE:
		FormatError(avahi_domain,
			    "Entry group failure: %s",
			    avahi_strerror(avahi_client_errno
					   (avahi_entry_group_get_client(g))));
		/* Some kind of failure happened while we were registering our services */
		avahi_running = false;
		break;

	case AVAHI_ENTRY_GROUP_UNCOMMITED:
		LogDebug(avahi_domain, "Service group is UNCOMMITED");
		break;
	case AVAHI_ENTRY_GROUP_REGISTERING:
		LogDebug(avahi_domain, "Service group is REGISTERING");
	}
}

/* Registers a new service with avahi */
static void avahiRegisterService(AvahiClient * c)
{
	FormatDebug(avahi_domain, "Registering service %s/%s",
		    SERVICE_TYPE, avahiName);

	int ret;
	assert(c);

	/* If this is the first time we're called,
	 * let's create a new entry group */
	if (!avahiGroup) {
		avahiGroup = avahi_entry_group_new(c, avahiGroupCallback, nullptr);
		if (!avahiGroup) {
			FormatError(avahi_domain,
				    "Failed to create avahi EntryGroup: %s",
				    avahi_strerror(avahi_client_errno(c)));
			goto fail;
		}
	}

	/* Add the service */
	/* TODO: This currently binds to ALL interfaces.
	 *       We could maybe add a service per actual bound interface,
	 *       if that's better. */
	ret = avahi_entry_group_add_service(avahiGroup,
					    AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
					    AvahiPublishFlags(0),
					    avahiName, SERVICE_TYPE, nullptr,
					    nullptr, listen_port, nullptr);
	if (ret < 0) {
		FormatError(avahi_domain, "Failed to add service %s: %s",
			    SERVICE_TYPE, avahi_strerror(ret));
		goto fail;
	}

	/* Tell the server to register the service group */
	ret = avahi_entry_group_commit(avahiGroup);
	if (ret < 0) {
		FormatError(avahi_domain, "Failed to commit service group: %s",
			    avahi_strerror(ret));
		goto fail;
	}
	return;

fail:
	avahi_running = false;
}

/* Callback when avahi changes state */
static void avahiClientCallback(AvahiClient * c, AvahiClientState state,
				gcc_unused void *userdata)
{
	int reason;
	assert(c);

	/* Called whenever the client or server state changes */
	FormatDebug(avahi_domain, "Client changed to state %d", state);

	switch (state) {
	case AVAHI_CLIENT_S_RUNNING:
		LogDebug(avahi_domain, "Client is RUNNING");

		/* The server has startup successfully and registered its host
		 * name on the network, so it's time to create our services */
		if (!avahiGroup)
			avahiRegisterService(c);
		break;

	case AVAHI_CLIENT_FAILURE:
		reason = avahi_client_errno(c);
		if (reason == AVAHI_ERR_DISCONNECTED) {
			LogDefault(avahi_domain,
				   "Client Disconnected, will reconnect shortly");
			if (avahiGroup) {
				avahi_entry_group_free(avahiGroup);
				avahiGroup = nullptr;
			}
			if (avahiClient)
				avahi_client_free(avahiClient);
			avahiClient =
			    avahi_client_new(avahi_poll,
					     AVAHI_CLIENT_NO_FAIL,
					     avahiClientCallback, nullptr,
					     &reason);
			if (!avahiClient) {
				FormatWarning(avahi_domain,
					      "Could not reconnect: %s",
					      avahi_strerror(reason));
				avahi_running = false;
			}
		} else {
			FormatWarning(avahi_domain,
				      "Client failure: %s (terminal)",
				      avahi_strerror(reason));
			avahi_running = false;
		}
		break;

	case AVAHI_CLIENT_S_COLLISION:
		LogDebug(avahi_domain, "Client is COLLISION");

		/* Let's drop our registered services. When the server is back
		 * in AVAHI_SERVER_RUNNING state we will register them
		 * again with the new host name. */
		if (avahiGroup) {
			LogDebug(avahi_domain, "Resetting group");
			avahi_entry_group_reset(avahiGroup);
		}

		break;

	case AVAHI_CLIENT_S_REGISTERING:
		LogDebug(avahi_domain, "Client is REGISTERING");

		/* The server records are now being established. This
		 * might be caused by a host name change. We need to wait
		 * for our own records to register until the host name is
		 * properly esatblished. */

		if (avahiGroup) {
			LogDebug(avahi_domain, "Resetting group");
			avahi_entry_group_reset(avahiGroup);
		}

		break;

	case AVAHI_CLIENT_CONNECTING:
		LogDebug(avahi_domain, "Client is CONNECTING");
		break;
	}
}

void
AvahiInit(EventLoop &loop, const char *serviceName)
{
	LogDebug(avahi_domain, "Initializing interface");

	if (!avahi_is_valid_service_name(serviceName))
		FormatFatalError("Invalid zeroconf_name \"%s\"", serviceName);

	avahiName = avahi_strdup(serviceName);

	avahi_running = true;

	avahi_poll = new MyAvahiPoll(loop);

	int error;
	avahiClient = avahi_client_new(avahi_poll, AVAHI_CLIENT_NO_FAIL,
				       avahiClientCallback, nullptr, &error);

	if (!avahiClient) {
		FormatError(avahi_domain, "Failed to create client: %s",
			    avahi_strerror(error));
		AvahiDeinit();
	}
}

void
AvahiDeinit(void)
{
	LogDebug(avahi_domain, "Shutting down interface");

	if (avahiGroup) {
		avahi_entry_group_free(avahiGroup);
		avahiGroup = nullptr;
	}

	if (avahiClient) {
		avahi_client_free(avahiClient);
		avahiClient = nullptr;
	}

	delete avahi_poll;
	avahi_poll = nullptr;

	avahi_free(avahiName);
	avahiName = nullptr;
}
