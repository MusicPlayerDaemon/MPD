// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OUTPUT_COMMANDS_HXX
#define MPD_OUTPUT_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

CommandResult
handle_enableoutput(Client &client, Request request, Response &response);

CommandResult
handle_disableoutput(Client &client, Request request, Response &response);

CommandResult
handle_toggleoutput(Client &client, Request request, Response &response);

CommandResult
handle_outputset(Client &client, Request request, Response &response);

CommandResult
handle_devices(Client &client, Request request, Response &response);

#endif
