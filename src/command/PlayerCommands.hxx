// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYER_COMMANDS_HXX
#define MPD_PLAYER_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

CommandResult
handle_play(Client &client, Request request, Response &response);

CommandResult
handle_playid(Client &client, Request request, Response &response);

CommandResult
handle_stop(Client &client, Request request, Response &response);

CommandResult
handle_currentsong(Client &client, Request request, Response &response);

CommandResult
handle_pause(Client &client, Request request, Response &response);

CommandResult
handle_status(Client &client, Request request, Response &response);

CommandResult
handle_next(Client &client, Request request, Response &response);

CommandResult
handle_previous(Client &client, Request request, Response &response);

CommandResult
handle_repeat(Client &client, Request request, Response &response);

CommandResult
handle_single(Client &client, Request request, Response &response);

CommandResult
handle_consume(Client &client, Request request, Response &response);

CommandResult
handle_random(Client &client, Request request, Response &response);

CommandResult
handle_clearerror(Client &client, Request request, Response &response);

CommandResult
handle_seek(Client &client, Request request, Response &response);

CommandResult
handle_seekid(Client &client, Request request, Response &response);

CommandResult
handle_seekcur(Client &client, Request request, Response &response);

CommandResult
handle_crossfade(Client &client, Request request, Response &response);

CommandResult
handle_mixrampdb(Client &client, Request request, Response &response);

CommandResult
handle_mixrampdelay(Client &client, Request request, Response &response);

CommandResult
handle_replay_gain_mode(Client &client, Request request, Response &response);

CommandResult
handle_replay_gain_status(Client &client, Request request, Response &response);

#endif
