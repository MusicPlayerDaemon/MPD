// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FILE_COMMANDS_HXX
#define MPD_FILE_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;
class Path;

CommandResult
handle_listfiles_local(Response &response, Path path_fs);

CommandResult
handle_read_comments(Client &client, Request request, Response &response);

CommandResult
handle_album_art(Client &client, Request request, Response &response);

CommandResult
handle_read_picture(Client &client, Request request, Response &response);

#endif
