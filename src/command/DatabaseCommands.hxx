// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
handle_searchcount(Client &client, Request request, Response &response);

CommandResult
handle_count(Client &client, Request request, Response &response);

CommandResult
handle_listall(Client &client, Request request, Response &response);

CommandResult
handle_list(Client &client, Request request, Response &response);

CommandResult
handle_listallinfo(Client &client, Request request, Response &response);

#endif
