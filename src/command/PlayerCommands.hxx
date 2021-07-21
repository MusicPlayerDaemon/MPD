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
