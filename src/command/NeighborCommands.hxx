// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NEIGHBOR_COMMANDS_HXX
#define MPD_NEIGHBOR_COMMANDS_HXX

#include "CommandResult.hxx"

struct Instance;
class Client;
class Request;
class Response;

[[gnu::pure]]
bool
neighbor_commands_available(const Instance &instance) noexcept;

CommandResult
handle_listneighbors(Client &client, Request request, Response &response);

#endif
