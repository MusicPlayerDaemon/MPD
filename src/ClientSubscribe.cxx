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

#include "ClientSubscribe.hxx"
#include "ClientIdle.hxx"
#include "ClientInternal.hxx"

extern "C" {
#include "idle.h"
}

#include <assert.h>
#include <string.h>

enum client_subscribe_result
client_subscribe(Client *client, const char *channel)
{
	assert(client != NULL);
	assert(channel != NULL);

	if (!client_message_valid_channel_name(channel))
		return CLIENT_SUBSCRIBE_INVALID;

	if (client->num_subscriptions >= CLIENT_MAX_SUBSCRIPTIONS)
		return CLIENT_SUBSCRIBE_FULL;

	auto r = client->subscriptions.insert(channel);
	if (!r.second)
		return CLIENT_SUBSCRIBE_ALREADY;

	++client->num_subscriptions;

	idle_add(IDLE_SUBSCRIPTION);

	return CLIENT_SUBSCRIBE_OK;
}

bool
client_unsubscribe(Client *client, const char *channel)
{
	const auto i = client->subscriptions.find(channel);
	if (i == client->subscriptions.end())
		return false;

	assert(client->num_subscriptions > 0);

	client->subscriptions.erase(i);
	--client->num_subscriptions;

	idle_add(IDLE_SUBSCRIPTION);

	assert((client->num_subscriptions == 0) ==
	       client->subscriptions.empty());

	return true;
}

void
client_unsubscribe_all(Client *client)
{
	client->subscriptions.clear();
	client->num_subscriptions = 0;
}

bool
client_push_message(Client *client, const ClientMessage &msg)
{
	assert(client != NULL);

	if (client->messages.size() >= CLIENT_MAX_MESSAGES ||
	    !client->IsSubscribed(msg.GetChannel()))
		return false;

	if (client->messages.empty())
		client_idle_add(client, IDLE_MESSAGE);

	client->messages.push_back(msg);
	return true;
}
