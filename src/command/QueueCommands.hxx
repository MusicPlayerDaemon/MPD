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
