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

#ifndef MPD_PLAYLIST_COMMANDS_HXX
#define MPD_PLAYLIST_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

[[gnu::const]]
bool
playlist_commands_available() noexcept;

CommandResult
handle_save(Client &client, Request request, Response &response);

CommandResult
handle_load(Client &client, Request request, Response &response);

CommandResult
handle_listplaylist(Client &client, Request request, Response &response);

CommandResult
handle_listplaylistinfo(Client &client, Request request, Response &response);

CommandResult
handle_rm(Client &client, Request request, Response &response);

CommandResult
handle_rename(Client &client, Request request, Response &response);

CommandResult
handle_playlistdelete(Client &client, Request request, Response &response);

CommandResult
handle_playlistmove(Client &client, Request request, Response &response);

CommandResult
handle_playlistclear(Client &client, Request request, Response &response);

CommandResult
handle_playlistadd(Client &client, Request request, Response &response);

CommandResult
handle_listplaylists(Client &client, Request request, Response &response);

#endif
