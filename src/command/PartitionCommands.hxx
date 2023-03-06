// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PARTITION_COMMANDS_HXX
#define MPD_PARTITION_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

CommandResult
handle_partition(Client &client, Request request, Response &response);

CommandResult
handle_listpartitions(Client &client, Request request, Response &response);

CommandResult
handle_newpartition(Client &client, Request request, Response &response);

CommandResult
handle_delpartition(Client &client, Request request, Response &response);

CommandResult
handle_moveoutput(Client &client, Request request, Response &response);

#endif
