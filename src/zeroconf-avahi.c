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

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/domain.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include <avahi-glib/glib-watch.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "avahi"

static char *avahiName;
static int avahiRunning;
static AvahiGLibPoll *avahi_glib_poll;
static const AvahiPoll *avahi_poll;
static AvahiClient *avahiClient;
static AvahiEntryGroup *avahiGroup;

static void avahiRegisterService(AvahiClient * c);

/* Callback when the EntryGroup changes state */
static void avahiGroupCallback(AvahiEntryGroup * g,
			       AvahiEntryGroupState state,
			       G_GNUC_UNUSED void *userdata)
{
	char *n;
	assert(g);

	g_debug("Service group changed to state %d", state);

	switch (state) {
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
		/* The entry group has been established successfully */
		g_message("Service '%s' successfully established.",
			  avahiName);
		break;

	case AVAHI_ENTRY_GROUP_COLLISION:
		/* A service name collision happened. Let's pick a new name */
		n = avahi_alternative_service_name(avahiName);
		avahi_free(avahiName);
		avahiName = n;

		g_message("Service name collision, renaming service to '%s'",
			  avahiName);

		/* And recreate the services */
		avahiRegisterService(avahi_entry_group_get_client(g));
		break;

	case AVAHI_ENTRY_GROUP_FAILURE:
		g_warning("Entry group failure: %s",
			  avahi_strerror(avahi_client_errno
					 (avahi_entry_group_get_client(g))));
		/* Some kind of failure happened while we were registering our services */
		avahiRunning = 0;
		break;

	case AVAHI_ENTRY_GROUP_UNCOMMITED:
		g_debug("Service group is UNCOMMITED");
		break;
	case AVAHI_ENTRY_GROUP_REGISTERING:
		g_debug("Service group is REGISTERING");
	}
}

/* Registers a new service with avahi */
static void avahiRegisterService(AvahiClient * c)
{
	int ret;
	assert(c);
	g_debug("Registering service %s/%s", SERVICE_TYPE, avahiName);

	/* If this is the first time we're called,
	 * let's create a new entry group */
	if (!avahiGroup) {
		avahiGroup = avahi_entry_group_new(c, avahiGroupCallback, NULL);
		if (!avahiGroup) {
			g_warning("Failed to create avahi EntryGroup: %s",
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
					    0, avahiName, SERVICE_TYPE, NULL,
					    NULL, listen_port, NULL);
	if (ret < 0) {
		g_warning("Failed to add service %s: %s", SERVICE_TYPE,
			  avahi_strerror(ret));
		goto fail;
	}

	/* Tell the server to register the service group */
	ret = avahi_entry_group_commit(avahiGroup);
	if (ret < 0) {
		g_warning("Failed to commit service group: %s",
			  avahi_strerror(ret));
		goto fail;
	}
	return;

fail:
	avahiRunning = 0;
}

/* Callback when avahi changes state */
static void avahiClientCallback(AvahiClient * c, AvahiClientState state,
				G_GNUC_UNUSED void *userdata)
{
	int reason;
	assert(c);

	/* Called whenever the client or server state changes */
	g_debug("Client changed to state %d", state);

	switch (state) {
	case AVAHI_CLIENT_S_RUNNING:
		g_debug("Client is RUNNING");

		/* The server has startup successfully and registered its host
		 * name on the network, so it's time to create our services */
		if (!avahiGroup)
			avahiRegisterService(c);
		break;

	case AVAHI_CLIENT_FAILURE:
		reason = avahi_client_errno(c);
		if (reason == AVAHI_ERR_DISCONNECTED) {
			g_message("Client Disconnected, will reconnect shortly");
			if (avahiGroup) {
				avahi_entry_group_free(avahiGroup);
				avahiGroup = NULL;
			}
			if (avahiClient)
				avahi_client_free(avahiClient);
			avahiClient =
			    avahi_client_new(avahi_poll,
					     AVAHI_CLIENT_NO_FAIL,
					     avahiClientCallback, NULL,
					     &reason);
			if (!avahiClient) {
				g_warning("Could not reconnect: %s",
					  avahi_strerror(reason));
				avahiRunning = 0;
			}
		} else {
			g_warning("Client failure: %s (terminal)",
				  avahi_strerror(reason));
			avahiRunning = 0;
		}
		break;

	case AVAHI_CLIENT_S_COLLISION:
		g_debug("Client is COLLISION");
		/* Let's drop our registered services. When the server is back
		 * in AVAHI_SERVER_RUNNING state we will register them
		 * again with the new host name. */
		if (avahiGroup) {
			g_debug("Resetting group");
			avahi_entry_group_reset(avahiGroup);
		}

	case AVAHI_CLIENT_S_REGISTERING:
		g_debug("Client is REGISTERING");
		/* The server records are now being established. This
		 * might be caused by a host name change. We need to wait
		 * for our own records to register until the host name is
		 * properly esatblished. */

		if (avahiGroup) {
			g_debug("Resetting group");
			avahi_entry_group_reset(avahiGroup);
		}

		break;

	case AVAHI_CLIENT_CONNECTING:
		g_debug("Client is CONNECTING");
	}
}

void init_avahi(const char *serviceName)
{
	int error;
	g_debug("Initializing interface");

	if (!avahi_is_valid_service_name(serviceName))
		g_error("Invalid zeroconf_name \"%s\"", serviceName);

	avahiName = avahi_strdup(serviceName);

	avahiRunning = 1;

	avahi_glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
	avahi_poll = avahi_glib_poll_get(avahi_glib_poll);

	avahiClient = avahi_client_new(avahi_poll, AVAHI_CLIENT_NO_FAIL,
				       avahiClientCallback, NULL, &error);

	if (!avahiClient) {
		g_warning("Failed to create client: %s",
			  avahi_strerror(error));
		goto fail;
	}

	return;

fail:
	avahi_finish();
}

void avahi_finish(void)
{
	g_debug("Shutting down interface");

	if (avahiGroup) {
		avahi_entry_group_free(avahiGroup);
		avahiGroup = NULL;
	}

	if (avahiClient) {
		avahi_client_free(avahiClient);
		avahiClient = NULL;
	}

	if (avahi_glib_poll != NULL) {
		avahi_glib_poll_free(avahi_glib_poll);
		avahi_glib_poll = NULL;
	}

	avahi_free(avahiName);
	avahiName = NULL;
}
