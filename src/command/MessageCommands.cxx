/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "MessageCommands.hxx"
#include "Request.hxx"
#include "client/Client.hxx"
#include "client/List.hxx"
#include "client/Response.hxx"
#include "util/ConstBuffer.hxx"
#include "Partition.hxx"

#include <fmt/format.h>

#include <cassert>
#include <set>
#include <string>

CommandResult
handle_subscribe(Client &client, Request args, Response &r)
{
	assert(args.size == 1);
	const char *const channel_name = args[0];

	switch (client.Subscribe(channel_name)) {
	case Client::SubscribeResult::OK:
		return CommandResult::OK;

	case Client::SubscribeResult::INVALID:
		r.Error(ACK_ERROR_ARG, "invalid channel name");
		return CommandResult::ERROR;

	case Client::SubscribeResult::ALREADY:
		r.Error(ACK_ERROR_EXIST, "already subscribed to this channel");
		return CommandResult::ERROR;

	case Client::SubscribeResult::FULL:
		r.Error(ACK_ERROR_EXIST, "subscription list is full");
		return CommandResult::ERROR;
	}

	/* unreachable */
	assert(false);
	gcc_unreachable();
}

CommandResult
handle_unsubscribe(Client &client, Request args, Response &r)
{
	assert(args.size == 1);
	const char *const channel_name = args[0];

	if (client.Unsubscribe(channel_name))
		return CommandResult::OK;
	else {
		r.Error(ACK_ERROR_NO_EXIST, "not subscribed to this channel");
		return CommandResult::ERROR;
	}
}

CommandResult
handle_channels(Client &client, [[maybe_unused]] Request args, Response &r)
{
	assert(args.empty());

	std::set<std::string> channels;

	for (const auto &c : client.GetPartition().clients) {
		const auto &subscriptions = c.GetSubscriptions();
		channels.insert(subscriptions.begin(),
				subscriptions.end());
	}

	for (const auto &channel : channels)
		r.Fmt(FMT_STRING("channel: {}\n"), channel);

	return CommandResult::OK;
}

CommandResult
handle_read_messages(Client &client,
		     [[maybe_unused]] Request args, Response &r)
{
	assert(args.empty());

	client.ConsumeMessages([&r](const auto &msg){
		r.Fmt(FMT_STRING("channel: {}\nmessage: {}\n"),
		      msg.GetChannel(), msg.GetMessage());
	});

	return CommandResult::OK;
}

CommandResult
handle_send_message(Client &client, Request args, Response &r)
{
	assert(args.size == 2);

	const char *const channel_name = args[0];
	const char *const message_text = args[1];

	if (!client_message_valid_channel_name(channel_name)) {
		r.Error(ACK_ERROR_ARG, "invalid channel name");
		return CommandResult::ERROR;
	}

	bool sent = false;
	const ClientMessage msg(channel_name, message_text);

	for (auto &c : client.GetPartition().clients)
		if (c.PushMessage(msg))
			sent = true;

	if (sent)
		return CommandResult::OK;
	else {
		r.Error(ACK_ERROR_NO_EXIST,
			"nobody is subscribed to this channel");
		return CommandResult::ERROR;
	}
}
