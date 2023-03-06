// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_QUEUE_COMMANDS_HXX
#define MPD_QUEUE_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

CommandResult
handle_add(Client &client, Request request, Response &response);

CommandResult
handle_addid(Client &client, Request request, Response &response);

CommandResult
handle_rangeid(Client &client, Request request, Response &response);

CommandResult
handle_delete(Client &client, Request request, Response &response);

CommandResult
handle_deleteid(Client &client, Request request, Response &response);

CommandResult
handle_playlist(Client &client, Request request, Response &response);

CommandResult
handle_shuffle(Client &client, Request request, Response &response);

CommandResult
handle_clear(Client &client, Request request, Response &response);

CommandResult
handle_plchanges(Client &client, Request request, Response &response);

CommandResult
handle_plchangesposid(Client &client, Request request, Response &response);

CommandResult
handle_playlistinfo(Client &client, Request request, Response &response);

CommandResult
handle_playlistid(Client &client, Request request, Response &response);

CommandResult
handle_playlistfind(Client &client, Request request, Response &response);

CommandResult
handle_playlistsearch(Client &client, Request request, Response &response);

CommandResult
handle_prio(Client &client, Request request, Response &response);

CommandResult
handle_prioid(Client &client, Request request, Response &response);

CommandResult
handle_move(Client &client, Request request, Response &response);

CommandResult
handle_moveid(Client &client, Request request, Response &response);

CommandResult
handle_swap(Client &client, Request request, Response &response);

CommandResult
handle_swapid(Client &client, Request request, Response &response);

#endif
