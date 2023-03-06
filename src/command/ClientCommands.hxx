// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CLIENT_COMMANDS_HXX
#define MPD_CLIENT_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

CommandResult
handle_close(Client &client, Request request, Response &response);

CommandResult
handle_ping(Client &client, Request request, Response &response);

CommandResult
handle_binary_limit(Client &client, Request request, Response &response);

CommandResult
handle_password(Client &client, Request request, Response &response);

CommandResult
handle_tagtypes(Client &client, Request request, Response &response);

#endif
