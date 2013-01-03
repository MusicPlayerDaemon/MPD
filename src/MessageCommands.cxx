/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

extern "C" {
#include "protocol/argparser.h"
#include "protocol/result.h"
#include "client_internal.h"
#include "client_subscribe.h"
}

#include <set>
#include <string>

#include <assert.h>

enum command_return
handle_subscribe(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
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
handle_unsubscribe(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
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
	const struct client *client = (const struct client *)data;

	for (GSList *i = client->subscriptions; i != NULL;
	     i = g_slist_next(i)) {
		const char *channel = (const char *)i->data;

		context->channels.insert(channel);
	}
}

enum command_return
handle_channels(struct client *client,
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
handle_read_messages(struct client *client,
		     G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	assert(argc == 1);

	GSList *messages = client_read_messages(client);

	for (GSList *i = messages; i != NULL; i = g_slist_next(i)) {
		struct client_message *msg = (struct client_message *)i->data;

		client_printf(client, "channel: %s\nmessage: %s\n",
			      msg->channel, msg->message);
		client_message_free(msg);
	}

	g_slist_free(messages);

	return COMMAND_RETURN_OK;
}

struct send_message_context {
	struct client_message msg;

	bool sent;
};

static void
send_message(gpointer data, gpointer user_data)
{
	struct send_message_context *context =
		(struct send_message_context *)user_data;
	struct client *client = (struct client *)data;

	if (client_push_message(client, &context->msg))
		context->sent = true;
}

enum command_return
handle_send_message(struct client *client,
		    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	assert(argc == 3);

	if (!client_message_valid_channel_name(argv[1])) {
		command_error(client, ACK_ERROR_ARG,
			      "invalid channel name");
		return COMMAND_RETURN_ERROR;
	}

	struct send_message_context context;
	context.sent = false;

	client_message_init(&context.msg, argv[1], argv[2]);

	client_list_foreach(send_message, &context);

	client_message_deinit(&context.msg);

	if (context.sent)
		return COMMAND_RETURN_OK;
	else {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "nobody is subscribed to this channel");
		return COMMAND_RETURN_ERROR;
	}
}
