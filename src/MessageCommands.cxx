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
#include "Client.hxx"
#include "ClientList.hxx"
#include "Instance.hxx"
#include "Main.hxx"
#include "protocol/Result.hxx"
#include "protocol/ArgParser.hxx"

#include <set>
#include <string>

#include <assert.h>

enum command_return
handle_subscribe(Client &client, gcc_unused int argc, char *argv[])
{
	assert(argc == 2);

	switch (client.Subscribe(argv[1])) {
	case Client::SubscribeResult::OK:
		return COMMAND_RETURN_OK;

	case Client::SubscribeResult::INVALID:
		command_error(client, ACK_ERROR_ARG,
			      "invalid channel name");
		return COMMAND_RETURN_ERROR;

	case Client::SubscribeResult::ALREADY:
		command_error(client, ACK_ERROR_EXIST,
			      "already subscribed to this channel");
		return COMMAND_RETURN_ERROR;

	case Client::SubscribeResult::FULL:
		command_error(client, ACK_ERROR_EXIST,
			      "subscription list is full");
		return COMMAND_RETURN_ERROR;
	}

	/* unreachable */
	assert(false);
	gcc_unreachable();
}

enum command_return
handle_unsubscribe(Client &client, gcc_unused int argc, char *argv[])
{
	assert(argc == 2);

	if (client.Unsubscribe(argv[1]))
		return COMMAND_RETURN_OK;
	else {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "not subscribed to this channel");
		return COMMAND_RETURN_ERROR;
	}
}

enum command_return
handle_channels(Client &client,
		gcc_unused int argc, gcc_unused char *argv[])
{
	assert(argc == 1);

	std::set<std::string> channels;
	for (const auto &c : *instance->client_list)
		channels.insert(c->subscriptions.begin(),
				c->subscriptions.end());

	for (const auto &channel : channels)
		client_printf(client, "channel: %s\n", channel.c_str());

	return COMMAND_RETURN_OK;
}

enum command_return
handle_read_messages(Client &client,
		     gcc_unused int argc, gcc_unused char *argv[])
{
	assert(argc == 1);

	while (!client.messages.empty()) {
		const ClientMessage &msg = client.messages.front();

		client_printf(client, "channel: %s\nmessage: %s\n",
			      msg.GetChannel(), msg.GetMessage());
		client.messages.pop_front();
	}

	return COMMAND_RETURN_OK;
}

enum command_return
handle_send_message(Client &client,
		    gcc_unused int argc, gcc_unused char *argv[])
{
	assert(argc == 3);

	if (!client_message_valid_channel_name(argv[1])) {
		command_error(client, ACK_ERROR_ARG,
			      "invalid channel name");
		return COMMAND_RETURN_ERROR;
	}

	bool sent = false;
	const ClientMessage msg(argv[1], argv[2]);
	for (const auto &c : *instance->client_list)
		if (c->PushMessage(msg))
			sent = true;

	if (sent)
		return COMMAND_RETURN_OK;
	else {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "nobody is subscribed to this channel");
		return COMMAND_RETURN_ERROR;
	}
}
