// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_MESSAGE_COMMANDS_HXX
#define MPD_MESSAGE_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

CommandResult
handle_subscribe(Client &client, Request request, Response &response);

CommandResult
handle_unsubscribe(Client &client, Request request, Response &response);

CommandResult
handle_channels(Client &client, Request request, Response &response);

CommandResult
handle_read_messages(Client &client, Request request, Response &response);

CommandResult
handle_send_message(Client &client, Request request, Response &response);

#endif
