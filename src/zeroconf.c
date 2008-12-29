/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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

#include "zeroconf.h"
#include "conf.h"
#include "listen.h"
#include "ioops.h"
#include "utils.h"

#include <glib.h>

#include <string.h>

/* The dns-sd service type qualifier to publish */
#define SERVICE_TYPE		"_mpd._tcp"

/* The default service name to publish
 * (overridden by 'zeroconf_name' config parameter)
 */
#define SERVICE_NAME		"Music Player"

#define DEFAULT_ZEROCONF_ENABLED 1

static int zeroconfEnabled;
static struct ioOps zeroConfIo;

#ifdef HAVE_BONJOUR
#include <dns_sd.h>

static DNSServiceRef dnsReference;
#endif

/* Here is the implementation for Avahi (http://avahi.org) Zeroconf support */
#ifdef HAVE_AVAHI

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/domain.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

/* Static avahi data */
static AvahiEntryGroup *avahiGroup;
static char *avahiName;
static AvahiClient *avahiClient;
static AvahiPoll avahiPoll;
static int avahiRunning;

static int avahiFdset(fd_set * rfds, fd_set * wfds, fd_set * efds);
static int avahiFdconsume(int fdCount, fd_set * rfds, fd_set * wfds,
			  fd_set * efds);

/* Forward Declaration */
static void avahiRegisterService(AvahiClient * c);

struct AvahiWatch {
	struct AvahiWatch *prev;
	struct AvahiWatch *next;
	int fd;
	AvahiWatchEvent requestedEvent;
	AvahiWatchEvent observedEvent;
	AvahiWatchCallback callback;
	void *userdata;
};

struct AvahiTimeout {
	struct AvahiTimeout *prev;
	struct AvahiTimeout *next;
	struct timeval expiry;
	int enabled;
	AvahiTimeoutCallback callback;
	void *userdata;
};

static AvahiWatch *avahiWatchList;
static AvahiTimeout *avahiTimeoutList;

static AvahiWatch *avahiWatchNew(G_GNUC_UNUSED const AvahiPoll * api, int fd,
				 AvahiWatchEvent event,
				 AvahiWatchCallback callback, void *userdata)
{
	struct AvahiWatch *newWatch = xmalloc(sizeof(struct AvahiWatch));

	newWatch->fd = fd;
	newWatch->requestedEvent = event;
	newWatch->observedEvent = 0;
	newWatch->callback = callback;
	newWatch->userdata = userdata;

	/* Insert at front of list */
	newWatch->next = avahiWatchList;
	avahiWatchList = newWatch;
	newWatch->prev = NULL;
	if (newWatch->next)
		newWatch->next->prev = newWatch;

	return newWatch;
}

static void avahiWatchUpdate(AvahiWatch * w, AvahiWatchEvent event)
{
	assert(w != NULL);
	w->requestedEvent = event;
}

static AvahiWatchEvent avahiWatchGetEvents(AvahiWatch * w)
{
	assert(w != NULL);
	return w->observedEvent;
}

static void avahiWatchFree(AvahiWatch * w)
{
	assert(w != NULL);

	if (avahiWatchList == w)
		avahiWatchList = w->next;
	else if (w->prev != NULL)
		w->prev->next = w->next;

	free(w);
}

static void avahiCheckExpiry(AvahiTimeout * t)
{
	assert(t != NULL);
	if (t->enabled) {
		struct timeval now;
		gettimeofday(&now, NULL);
		if (timercmp(&now, &(t->expiry), >)) {
			t->enabled = 0;
			t->callback(t, t->userdata);
		}
	}
}

static void avahiTimeoutUpdate(AvahiTimeout * t, const struct timeval *tv)
{
	assert(t != NULL);
	if (tv) {
		t->enabled = 1;
		t->expiry.tv_sec = tv->tv_sec;
		t->expiry.tv_usec = tv->tv_usec;
	} else {
		t->enabled = 0;
	}
}

static void avahiTimeoutFree(AvahiTimeout * t)
{
	assert(t != NULL);

	if (avahiTimeoutList == t)
		avahiTimeoutList = t->next;
	else if (t->prev != NULL)
		t->prev->next = t->next;

	free(t);
}

static AvahiTimeout *avahiTimeoutNew(G_GNUC_UNUSED const AvahiPoll * api,
				     const struct timeval *tv,
				     AvahiTimeoutCallback callback,
				     void *userdata)
{
	struct AvahiTimeout *newTimeout = xmalloc(sizeof(struct AvahiTimeout));

	newTimeout->callback = callback;
	newTimeout->userdata = userdata;

	avahiTimeoutUpdate(newTimeout, tv);

	/* Insert at front of list */
	newTimeout->next = avahiTimeoutList;
	avahiTimeoutList = newTimeout;
	newTimeout->prev = NULL;
	if (newTimeout->next)
		newTimeout->next->prev = newTimeout;

	return newTimeout;
}

/* Callback when the EntryGroup changes state */
static void avahiGroupCallback(AvahiEntryGroup * g,
			       AvahiEntryGroupState state,
			       G_GNUC_UNUSED void *userdata)
{
	char *n;
	assert(g);

	g_debug("Avahi: Service group changed to state %d", state);

	switch (state) {
	case AVAHI_ENTRY_GROUP_ESTABLISHED:
		/* The entry group has been established successfully */
		g_message("Avahi: Service '%s' successfully established.",
			  avahiName);
		break;

	case AVAHI_ENTRY_GROUP_COLLISION:
		/* A service name collision happened. Let's pick a new name */
		n = avahi_alternative_service_name(avahiName);
		avahi_free(avahiName);
		avahiName = n;

		g_message("Avahi: Service name collision, renaming service to '%s'",
			  avahiName);

		/* And recreate the services */
		avahiRegisterService(avahi_entry_group_get_client(g));
		break;

	case AVAHI_ENTRY_GROUP_FAILURE:
		g_warning("Avahi: Entry group failure: %s",
			  avahi_strerror(avahi_client_errno
					 (avahi_entry_group_get_client(g))));
		/* Some kind of failure happened while we were registering our services */
		avahiRunning = 0;
		break;

	case AVAHI_ENTRY_GROUP_UNCOMMITED:
		g_debug("Avahi: Service group is UNCOMMITED");
		break;
	case AVAHI_ENTRY_GROUP_REGISTERING:
		g_debug("Avahi: Service group is REGISTERING");
	}
}

/* Registers a new service with avahi */
static void avahiRegisterService(AvahiClient * c)
{
	int ret;
	assert(c);
	g_debug("Avahi: Registering service %s/%s", SERVICE_TYPE, avahiName);

	/* If this is the first time we're called,
	 * let's create a new entry group */
	if (!avahiGroup) {
		avahiGroup = avahi_entry_group_new(c, avahiGroupCallback, NULL);
		if (!avahiGroup) {
			g_warning("Avahi: Failed to create avahi EntryGroup: %s",
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
					    NULL, boundPort, NULL);
	if (ret < 0) {
		g_warning("Avahi: Failed to add service %s: %s", SERVICE_TYPE,
			  avahi_strerror(ret));
		goto fail;
	}

	/* Tell the server to register the service group */
	ret = avahi_entry_group_commit(avahiGroup);
	if (ret < 0) {
		g_warning("Avahi: Failed to commit service group: %s",
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
	g_debug("Avahi: Client changed to state %d", state);

	switch (state) {
	case AVAHI_CLIENT_S_RUNNING:
		g_debug("Avahi: Client is RUNNING");

		/* The server has startup successfully and registered its host
		 * name on the network, so it's time to create our services */
		if (!avahiGroup)
			avahiRegisterService(c);
		break;

	case AVAHI_CLIENT_FAILURE:
		reason = avahi_client_errno(c);
		if (reason == AVAHI_ERR_DISCONNECTED) {
			g_message("Avahi: Client Disconnected, "
				  "will reconnect shortly");
			if (avahiGroup) {
				avahi_entry_group_free(avahiGroup);
				avahiGroup = NULL;
			}
			if (avahiClient)
				avahi_client_free(avahiClient);
			avahiClient =
			    avahi_client_new(&avahiPoll,
					     AVAHI_CLIENT_NO_FAIL,
					     avahiClientCallback, NULL,
					     &reason);
			if (!avahiClient) {
				g_warning("Avahi: Could not reconnect: %s",
					  avahi_strerror(reason));
				avahiRunning = 0;
			}
		} else {
			g_warning("Avahi: Client failure: %s (terminal)",
				  avahi_strerror(reason));
			avahiRunning = 0;
		}
		break;

	case AVAHI_CLIENT_S_COLLISION:
		g_debug("Avahi: Client is COLLISION");
		/* Let's drop our registered services. When the server is back
		 * in AVAHI_SERVER_RUNNING state we will register them
		 * again with the new host name. */
		if (avahiGroup) {
			g_debug("Avahi: Resetting group");
			avahi_entry_group_reset(avahiGroup);
		}

	case AVAHI_CLIENT_S_REGISTERING:
		g_debug("Avahi: Client is REGISTERING");
		/* The server records are now being established. This
		 * might be caused by a host name change. We need to wait
		 * for our own records to register until the host name is
		 * properly esatblished. */

		if (avahiGroup) {
			g_debug("Avahi: Resetting group");
			avahi_entry_group_reset(avahiGroup);
		}

		break;

	case AVAHI_CLIENT_CONNECTING:
		g_debug("Avahi: Client is CONNECTING");
	}
}

static int avahiFdset(fd_set * rfds, fd_set * wfds, fd_set * efds)
{
	AvahiWatch *w;
	int maxfd = -1;
	if (!avahiRunning)
		return maxfd;
	for (w = avahiWatchList; w != NULL; w = w->next) {
		if (w->requestedEvent & AVAHI_WATCH_IN) {
			FD_SET(w->fd, rfds);
		}
		if (w->requestedEvent & AVAHI_WATCH_OUT) {
			FD_SET(w->fd, wfds);
		}
		if (w->requestedEvent & AVAHI_WATCH_ERR) {
			FD_SET(w->fd, efds);
		}
		if (w->requestedEvent & AVAHI_WATCH_HUP) {
			g_warning("Avahi: No support for HUP events! (ignoring)");
		}

		if (w->fd > maxfd)
			maxfd = w->fd;
	}
	return maxfd;
}

static int avahiFdconsume(int fdCount, fd_set * rfds, fd_set * wfds,
			  fd_set * efds)
{
	int retval = fdCount;
	AvahiTimeout *t;
	AvahiWatch *w = avahiWatchList;

	while (w != NULL && retval > 0) {
		AvahiWatch *current = w;
		current->observedEvent = 0;
		if (FD_ISSET(current->fd, rfds)) {
			current->observedEvent |= AVAHI_WATCH_IN;
			FD_CLR(current->fd, rfds);
			retval--;
		}
		if (FD_ISSET(current->fd, wfds)) {
			current->observedEvent |= AVAHI_WATCH_OUT;
			FD_CLR(current->fd, wfds);
			retval--;
		}
		if (FD_ISSET(current->fd, efds)) {
			current->observedEvent |= AVAHI_WATCH_ERR;
			FD_CLR(current->fd, efds);
			retval--;
		}

		/* Advance to the next one right now, in case the callback
		 * removes itself
		 */
		w = w->next;

		if (current->observedEvent && avahiRunning) {
			current->callback(current, current->fd,
					  current->observedEvent,
					  current->userdata);
		}
	}

	t = avahiTimeoutList;
	while (t != NULL && avahiRunning) {
		AvahiTimeout *current = t;

		/* Advance to the next one right now, in case the callback
		 * removes itself
		 */
		t = t->next;
		avahiCheckExpiry(current);
	}

	return retval;
}

static void init_avahi(const char *serviceName)
{
	int error;
	g_debug("Avahi: Initializing interface");

	if (avahi_is_valid_service_name(serviceName)) {
		avahiName = avahi_strdup(serviceName);
	} else {
		g_warning("Invalid zeroconf_name \"%s\", defaulting to "
			  "\"%s\" instead.",
			  serviceName, SERVICE_NAME);
		avahiName = avahi_strdup(SERVICE_NAME);
	}

	avahiRunning = 1;

	avahiPoll.userdata = NULL;
	avahiPoll.watch_new = avahiWatchNew;
	avahiPoll.watch_update = avahiWatchUpdate;
	avahiPoll.watch_get_events = avahiWatchGetEvents;
	avahiPoll.watch_free = avahiWatchFree;
	avahiPoll.timeout_new = avahiTimeoutNew;
	avahiPoll.timeout_update = avahiTimeoutUpdate;
	avahiPoll.timeout_free = avahiTimeoutFree;

	avahiClient = avahi_client_new(&avahiPoll, AVAHI_CLIENT_NO_FAIL,
				       avahiClientCallback, NULL, &error);

	if (!avahiClient) {
		g_warning("Avahi: Failed to create client: %s",
			  avahi_strerror(error));
		goto fail;
	}

	zeroConfIo.fdset = avahiFdset;
	zeroConfIo.consume = avahiFdconsume;
	registerIO(&zeroConfIo);

	return;

fail:
	finishZeroconf();
}
#endif /* HAVE_AVAHI */

#ifdef HAVE_BONJOUR
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

static void init_zeroconf_osx(const char *serviceName)
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
#endif

void initZeroconf(void)
{
	const char *serviceName = SERVICE_NAME;
	ConfigParam *param;

	zeroconfEnabled = getBoolConfigParam(CONF_ZEROCONF_ENABLED, 1);
	if (zeroconfEnabled == CONF_BOOL_UNSET)
		zeroconfEnabled = DEFAULT_ZEROCONF_ENABLED;

	if (!zeroconfEnabled)
		return;

	param = getConfigParam(CONF_ZEROCONF_NAME);

	if (param && strlen(param->value) > 0)
		serviceName = param->value;

#ifdef HAVE_AVAHI
	init_avahi(serviceName);
#endif

#ifdef HAVE_BONJOUR
	init_zeroconf_osx(serviceName);
#endif
}

void finishZeroconf(void)
{
	if (!zeroconfEnabled)
		return;

#ifdef HAVE_AVAHI
	g_debug("Avahi: Shutting down interface");
	deregisterIO(&zeroConfIo);

	if (avahiGroup) {
		avahi_entry_group_free(avahiGroup);
		avahiGroup = NULL;
	}

	if (avahiClient) {
		avahi_client_free(avahiClient);
		avahiClient = NULL;
	}

	avahi_free(avahiName);
	avahiName = NULL;
#endif /* HAVE_AVAHI */

#ifdef HAVE_BONJOUR
	deregisterIO(&zeroConfIo);
	if (dnsReference != NULL) {
		DNSServiceRefDeallocate(dnsReference);
		dnsReference = NULL;
		g_debug("Deregistered Zeroconf service.");
	}
#endif
}
