// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OTHER_COMMANDS_HXX
#define MPD_OTHER_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

CommandResult
handle_urlhandlers(Client &client, Request request, Response &response);

CommandResult
handle_decoders(Client &client, Request request, Response &response);

CommandResult
handle_kill(Client &client, Request request, Response &response);

CommandResult
handle_listfiles(Client &client, Request request, Response &response);

CommandResult
handle_lsinfo(Client &client, Request request, Response &response);

CommandResult
handle_update(Client &client, Request request, Response &response);

CommandResult
handle_rescan(Client &client, Request request, Response &response);

CommandResult
handle_getvol(Client &client, Request request, Response &response);

CommandResult
handle_setvol(Client &client, Request request, Response &response);

CommandResult
handle_volume(Client &client, Request request, Response &response);

CommandResult
handle_stats(Client &client, Request request, Response &response);

CommandResult
handle_config(Client &client, Request request, Response &response);

CommandResult
handle_idle(Client &client, Request request, Response &response);

#endif
