/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "ZeroconfAvahi.hxx"
#include "AvahiPoll.hxx"
#include "ZeroconfInternal.hxx"
#include "Listen.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/domain.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

static constexpr Domain avahi_domain("avahi");

static char *avahi_name;
static MyAvahiPoll *avahi_poll;
static AvahiClient *avahi_client;
static AvahiEntryGroup *avahi_group;

static void
AvahiRegisterService(AvahiClient *c);

/**
 * Callback when the EntryGroup changes state.
 */
static void
AvahiGroupCallback(AvahiEntryGroup *g,
		   AvahiEntryGroupState state,
		   gcc_unused void *userdata)
{
	assert(g != nullptr);

	FormatDebug(avahi_domain,
		    "Service group changed to state %d", state);

	switch (state) {
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
		/* The entry group has been established successfully */
		FormatDefault(avahi_domain,
			      "Service '%s' successfully established.",
			      avahi_name);
		break;

	case AVAHI_ENTRY_GROUP_COLLISION:
		/* A service name collision happened. Let's pick a new name */
		{
			char *n = avahi_alternative_service_name(avahi_name);
			avahi_free(avahi_name);
			avahi_name = n;
		}

		FormatDefault(avahi_domain,
			      "Service name collision, renaming service to '%s'",
			      avahi_name);

		/* And recreate the services */
		AvahiRegisterService(avahi_entry_group_get_client(g));
		break;

	case AVAHI_ENTRY_GROUP_FAILURE:
		FormatError(avahi_domain,
			    "Entry group failure: %s",
			    avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));
		/* Some kind of failure happened while we were
		   registering our services */
		break;

	case AVAHI_ENTRY_GROUP_UNCOMMITED:
		LogDebug(avahi_domain, "Service group is UNCOMMITED");
		break;

	case AVAHI_ENTRY_GROUP_REGISTERING:
		LogDebug(avahi_domain, "Service group is REGISTERING");
	}
}

/**
 * Registers a new service with avahi.
 */
static void
AvahiRegisterService(AvahiClient *c)
{
	assert(c != nullptr);

	FormatDebug(avahi_domain, "Registering service %s/%s",
		    SERVICE_TYPE, avahi_name);

	/* If this is the first time we're called,
	 * let's create a new entry group */
	if (!avahi_group) {
		avahi_group = avahi_entry_group_new(c, AvahiGroupCallback, nullptr);
		if (!avahi_group) {
			FormatError(avahi_domain,
				    "Failed to create avahi EntryGroup: %s",
				    avahi_strerror(avahi_client_errno(c)));
			return;
		}
	}

	/* Add the service */
	/* TODO: This currently binds to ALL interfaces.
	 *       We could maybe add a service per actual bound interface,
	 *       if that's better. */
	int result = avahi_entry_group_add_service(avahi_group,
						   AVAHI_IF_UNSPEC,
						   AVAHI_PROTO_UNSPEC,
						   AvahiPublishFlags(0),
						   avahi_name, SERVICE_TYPE,
						   nullptr, nullptr,
						   listen_port, nullptr);
	if (result < 0) {
		FormatError(avahi_domain, "Failed to add service %s: %s",
			    SERVICE_TYPE, avahi_strerror(result));
		return;
	}

	/* Tell the server to register the service group */
	result = avahi_entry_group_commit(avahi_group);
	if (result < 0) {
		FormatError(avahi_domain, "Failed to commit service group: %s",
			    avahi_strerror(result));
		return;
	}
}

/* Callback when avahi changes state */
static void
MyAvahiClientCallback(AvahiClient *c, AvahiClientState state,
		      gcc_unused void *userdata)
{
	assert(c != nullptr);

	/* Called whenever the client or server state changes */
	FormatDebug(avahi_domain, "Client changed to state %d", state);

	switch (state) {
		int reason;

	case AVAHI_CLIENT_S_RUNNING:
		LogDebug(avahi_domain, "Client is RUNNING");

		/* The server has startup successfully and registered its host
		 * name on the network, so it's time to create our services */
		if (avahi_group == nullptr)
			AvahiRegisterService(c);
		break;

	case AVAHI_CLIENT_FAILURE:
		reason = avahi_client_errno(c);
		if (reason == AVAHI_ERR_DISCONNECTED) {
			LogDefault(avahi_domain,
				   "Client Disconnected, will reconnect shortly");
			if (avahi_group != nullptr) {
				avahi_entry_group_free(avahi_group);
				avahi_group = nullptr;
			}

			if (avahi_client != nullptr)
				avahi_client_free(avahi_client);
			avahi_client =
			    avahi_client_new(avahi_poll,
					     AVAHI_CLIENT_NO_FAIL,
					     MyAvahiClientCallback, nullptr,
					     &reason);
			if (avahi_client == nullptr)
				FormatWarning(avahi_domain,
					      "Could not reconnect: %s",
					      avahi_strerror(reason));
		} else {
			FormatWarning(avahi_domain,
				      "Client failure: %s (terminal)",
				      avahi_strerror(reason));
		}

		break;

	case AVAHI_CLIENT_S_COLLISION:
		LogDebug(avahi_domain, "Client is COLLISION");

		/* Let's drop our registered services. When the server
		   is back in AVAHI_SERVER_RUNNING state we will
		   register them again with the new host name. */
		if (avahi_group != nullptr) {
			LogDebug(avahi_domain, "Resetting group");
			avahi_entry_group_reset(avahi_group);
		}

		break;

	case AVAHI_CLIENT_S_REGISTERING:
		LogDebug(avahi_domain, "Client is REGISTERING");

		/* The server records are now being established. This
		 * might be caused by a host name change. We need to wait
		 * for our own records to register until the host name is
		 * properly esatblished. */

		if (avahi_group != nullptr) {
			LogDebug(avahi_domain, "Resetting group");
			avahi_entry_group_reset(avahi_group);
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
		throw FormatRuntimeError("Invalid zeroconf_name \"%s\"", serviceName);

	avahi_name = avahi_strdup(serviceName);

	avahi_poll = new MyAvahiPoll(loop);

	int error;
	avahi_client = avahi_client_new(avahi_poll, AVAHI_CLIENT_NO_FAIL,
					MyAvahiClientCallback, nullptr,
					&error);
	if (avahi_client == nullptr) {
		FormatError(avahi_domain, "Failed to create client: %s",
			    avahi_strerror(error));
		AvahiDeinit();
	}
}

void
AvahiDeinit()
{
	LogDebug(avahi_domain, "Shutting down interface");

	if (avahi_group != nullptr) {
		avahi_entry_group_free(avahi_group);
		avahi_group = nullptr;
	}

	if (avahi_client != nullptr) {
		avahi_client_free(avahi_client);
		avahi_client = nullptr;
	}

	delete avahi_poll;
	avahi_poll = nullptr;

	avahi_free(avahi_name);
	avahi_name = nullptr;
}
