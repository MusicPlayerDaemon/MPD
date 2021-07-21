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

#ifndef MPD_DATABASE_COMMANDS_HXX
#define MPD_DATABASE_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

CommandResult
handle_listfiles_db(Client &client, Response &r, const char *uri);

CommandResult
handle_lsinfo2(Client &client, const char *uri, Response &response);

CommandResult
handle_find(Client &client, Request request, Response &response);

CommandResult
handle_findadd(Client &client, Request request, Response &response);

CommandResult
handle_search(Client &client, Request request, Response &response);

CommandResult
handle_searchadd(Client &client, Request request, Response &response);

CommandResult
handle_searchaddpl(Client &client, Request request, Response &response);

CommandResult
handle_count(Client &client, Request request, Response &response);

CommandResult
handle_listall(Client &client, Request request, Response &response);

CommandResult
handle_list(Client &client, Request request, Response &response);

CommandResult
handle_listallinfo(Client &client, Request request, Response &response);

#endif
