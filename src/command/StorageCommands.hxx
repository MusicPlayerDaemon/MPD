// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "CommandResult.hxx"

#include <string_view>

struct Instance;
class Client;
class Storage;
class Request;
class Response;

CommandResult
handle_listfiles_storage(Response &r, Storage &storage, std::string_view uri);

CommandResult
handle_listfiles_storage(Client &client, Response &r, std::string_view uri);

CommandResult
handle_listmounts(Client &client, Request request, Response &response);

CommandResult
handle_mount(Client &client, Request request, Response &response);

CommandResult
handle_unmount(Client &client, Request request, Response &response);

[[gnu::pure]]
bool
mount_commands_available(Instance &instance) noexcept;
