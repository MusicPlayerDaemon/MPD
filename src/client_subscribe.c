/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#include "client_subscribe.h"
#include "client_internal.h"
#include "client_idle.h"
#include "idle.h"

#include <string.h>

G_GNUC_PURE
static GSList *
client_find_subscription(const struct client *client, const char *channel)
{
	for (GSList *i = client->subscriptions; i != NULL; i = g_slist_next(i))
		if (strcmp((const char *)i->data, channel) == 0)
			return i;

	return NULL;
}

enum client_subscribe_result
client_subscribe(struct client *client, const char *channel)
{
	assert(client != NULL);
	assert(channel != NULL);

	if (!client_message_valid_channel_name(channel))
		return CLIENT_SUBSCRIBE_INVALID;

	if (client_find_subscription(client, channel) != NULL)
		return CLIENT_SUBSCRIBE_ALREADY;

	if (client->num_subscriptions >= CLIENT_MAX_SUBSCRIPTIONS)
		return CLIENT_SUBSCRIBE_FULL;

	client->subscriptions = g_slist_prepend(client->subscriptions,
						g_strdup(channel));
	++client->num_subscriptions;

	idle_add(IDLE_SUBSCRIPTION);

	return CLIENT_SUBSCRIBE_OK;
}

bool
client_unsubscribe(struct client *client, const char *channel)
{
	GSList *i = client_find_subscription(client, channel);
	if (i == NULL)
		return false;

	assert(client->num_subscriptions > 0);

	client->subscriptions = g_slist_remove(client->subscriptions, i->data);
	--client->num_subscriptions;

	idle_add(IDLE_SUBSCRIPTION);

	assert((client->num_subscriptions == 0) ==
	       (client->subscriptions == NULL));

	return true;
}

void
client_unsubscribe_all(struct client *client)
{
	for (GSList *i = client->subscriptions; i != NULL; i = g_slist_next(i))
		g_free(i->data);

	g_slist_free(client->subscriptions);
	client->subscriptions = NULL;
	client->num_subscriptions = 0;
}

bool
client_push_message(struct client *client, const struct client_message *msg)
{
	assert(client != NULL);
	assert(msg != NULL);
	assert(client_message_defined(msg));

	if (client->num_messages >= CLIENT_MAX_MESSAGES ||
	    client_find_subscription(client, msg->channel) == NULL)
		return false;

	if (client->messages == NULL)
		client_idle_add(client, IDLE_MESSAGE);

	client->messages = g_slist_prepend(client->messages,
					   client_message_dup(msg));
	++client->num_messages;

	return true;
}

GSList *
client_read_messages(struct client *client)
{
	GSList *messages = g_slist_reverse(client->messages);

	client->messages = NULL;
	client->num_messages = 0;

	return messages;
}
