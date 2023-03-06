// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FINGERPRINT_COMMANDS_HXX
#define MPD_FINGERPRINT_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;
class Response;

CommandResult
handle_getfingerprint(Client &client, Request request, Response &response);

#endif
