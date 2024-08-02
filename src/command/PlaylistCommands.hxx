// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
handle_searchplaylist(Client &client, Request request, Response &response);

CommandResult
handle_playlistlength(Client &client, Request request, Response &response);

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
