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
#include "MessageCommands.hxx"
#include "ClientSubscribe.hxx"
#include "ClientInternal.hxx"
#include "ClientList.hxx"
#include "protocol/Result.hxx"
#include "protocol/ArgParser.hxx"

#include <set>
#include <string>

#include <assert.h>

enum command_return
handle_subscribe(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	assert(argc == 2);

	switch (client_subscribe(client, argv[1])) {
	case CLIENT_SUBSCRIBE_OK:
		return COMMAND_RETURN_OK;

	case CLIENT_SUBSCRIBE_INVALID:
		command_error(client, ACK_ERROR_ARG,
			      "invalid channel name");
		return COMMAND_RETURN_ERROR;

	case CLIENT_SUBSCRIBE_ALREADY:
		command_error(client, ACK_ERROR_EXIST,
			      "already subscribed to this channel");
		return COMMAND_RETURN_ERROR;

	case CLIENT_SUBSCRIBE_FULL:
		command_error(client, ACK_ERROR_EXIST,
			      "subscription list is full");
		return COMMAND_RETURN_ERROR;
	}

	/* unreachable */
	return COMMAND_RETURN_OK;
}

enum command_return
handle_unsubscribe(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	assert(argc == 2);

	if (client_unsubscribe(client, argv[1]))
		return COMMAND_RETURN_OK;
	else {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "not subscribed to this channel");
		return COMMAND_RETURN_ERROR;
	}
}

struct channels_context {
	std::set<std::string> channels;
};

static void
collect_channels(gpointer data, gpointer user_data)
{
	struct channels_context *context =
		(struct channels_context *)user_data;
	const Client *client = (const Client *)data;

	context->channels.insert(client->subscriptions.begin(),
				 client->subscriptions.end());
}

enum command_return
handle_channels(Client *client,
		G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	assert(argc == 1);

	struct channels_context context;

	client_list_foreach(collect_channels, &context);

	for (const auto &channel : context.channels)
		client_printf(client, "channel: %s\n", channel.c_str());

	return COMMAND_RETURN_OK;
}

enum command_return
handle_read_messages(Client *client,
		     G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	assert(argc == 1);

	while (!client->messages.empty()) {
		const ClientMessage &msg = client->messages.front();

		client_printf(client, "channel: %s\nmessage: %s\n",
			      msg.GetChannel(), msg.GetMessage());
		client->messages.pop_front();
	}

	return COMMAND_RETURN_OK;
}

struct send_message_context {
	ClientMessage msg;

	bool sent;

	template<typename T, typename U>
	send_message_context(T &&_channel, U &&_message)
		:msg(std::forward<T>(_channel), std::forward<U>(_message)),
		 sent(false) {}
};

static void
send_message(gpointer data, gpointer user_data)
{
	struct send_message_context *context =
		(struct send_message_context *)user_data;
	Client *client = (Client *)data;

	if (client_push_message(client, context->msg))
		context->sent = true;
}

enum command_return
handle_send_message(Client *client,
		    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	assert(argc == 3);

	if (!client_message_valid_channel_name(argv[1])) {
		command_error(client, ACK_ERROR_ARG,
			      "invalid channel name");
		return COMMAND_RETURN_ERROR;
	}

	struct send_message_context context(argv[1], argv[2]);

	client_list_foreach(send_message, &context);

	if (context.sent)
		return COMMAND_RETURN_OK;
	else {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "nobody is subscribed to this channel");
		return COMMAND_RETURN_ERROR;
	}
}
