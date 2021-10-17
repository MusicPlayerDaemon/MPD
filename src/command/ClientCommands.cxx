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

#include "ClientCommands.hxx"
#include "Request.hxx"
#include "Permission.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "TagPrint.hxx"
#include "tag/ParseName.hxx"
#include "util/StringAPI.hxx"

CommandResult
handle_close([[maybe_unused]] Client &client, [[maybe_unused]] Request args,
	     [[maybe_unused]] Response &r)
{
	return CommandResult::FINISH;
}

CommandResult
handle_ping([[maybe_unused]] Client &client, [[maybe_unused]] Request args,
	    [[maybe_unused]] Response &r)
{
	return CommandResult::OK;
}

CommandResult
handle_binary_limit(Client &client, Request args,
		    [[maybe_unused]] Response &r)
{
	size_t value = args.ParseUnsigned(0, client.GetOutputMaxSize() - 4096);
	if (value < 64) {
		r.Error(ACK_ERROR_ARG, "Value too small");
		return CommandResult::ERROR;
	}

	client.binary_limit = value;

	return CommandResult::OK;
}

CommandResult
handle_password(Client &client, Request args, Response &r)
{
	const auto permission = GetPermissionFromPassword(args.front());
	if (!permission) {
		r.Error(ACK_ERROR_PASSWORD, "incorrect password");
		return CommandResult::ERROR;
	}

	client.SetPermission(*permission);

	return CommandResult::OK;
}

static TagMask
ParseTagMask(Request request)
{
	if (request.empty())
		throw ProtocolError(ACK_ERROR_ARG, "Not enough arguments");

	TagMask result = TagMask::None();

	for (const char *name : request) {
		auto type = tag_name_parse_i(name);
		if (type == TAG_NUM_OF_ITEM_TYPES)
			throw ProtocolError(ACK_ERROR_ARG, "Unknown tag type");

		result |= type;
	}

	return result;
}

CommandResult
handle_tagtypes(Client &client, Request request, Response &r)
{
	if (request.empty()) {
		tag_print_types(r);
		return CommandResult::OK;
	}

	const char *cmd = request.shift();
	if (StringIsEqual(cmd, "all")) {
		if (!request.empty()) {
			r.Error(ACK_ERROR_ARG, "Too many arguments");
			return CommandResult::ERROR;
		}

		client.tag_mask = TagMask::All();
		return CommandResult::OK;
	} else if (StringIsEqual(cmd, "clear")) {
		if (!request.empty()) {
			r.Error(ACK_ERROR_ARG, "Too many arguments");
			return CommandResult::ERROR;
		}

		client.tag_mask = TagMask::None();
		return CommandResult::OK;
	} else if (StringIsEqual(cmd, "enable")) {
		client.tag_mask |= ParseTagMask(request);
		return CommandResult::OK;
	} else if (StringIsEqual(cmd, "disable")) {
		client.tag_mask &= ~ParseTagMask(request);
		return CommandResult::OK;
	} else {
		r.Error(ACK_ERROR_ARG, "Unknown sub command");
		return CommandResult::ERROR;
	}
}
