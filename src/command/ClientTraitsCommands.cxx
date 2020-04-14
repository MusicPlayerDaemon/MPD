/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "ClientTraitsCommands.hxx"

#include "Request.hxx"
#include "client/Response.hxx"
#include "client/Client.hxx"

#include "util/StringCompare.hxx"

static CommandResult
handle_list(Client &client, Request request, Response &response)
{
	if (!request.empty()) {
		response.FormatError(ACK_ERROR_ARG, "Too many arguments: %s", ClientTraits::COMMAND_SYNTAX);
		return CommandResult::ERROR;
	}

	const ClientTraits& traits = client.GetTraits();

	for (ClientTraits::Trait trait: ClientTraits::s_all_traits) {
		response.Format("%s: %s\n", traits.trait_name(trait), traits.trait_value(trait));
	}

	return CommandResult::OK;
}

static CommandResult
handle_get(Client &client, Request request, Response &response)
{
	if (request.size != 1) {
		response.FormatError(ACK_ERROR_ARG, "Wrong number of arguments: %s", ClientTraits::COMMAND_SYNTAX);
		return CommandResult::ERROR;
	}
	
	const char* trait_name = request.shift();
	if (trait_name == nullptr)
		trait_name = "";
	
	const ClientTraits& traits = client.GetTraits();
	
	ClientTraits::Trait trait = traits.trait(trait_name);
	if (trait == ClientTraits::Trait::Invalid) {
		response.FormatError(ACK_ERROR_ARG, "Unknown trait name: \"%s\"", trait_name);
		return CommandResult::ERROR;
	}
	
	const char* trait_value = traits.trait_value(trait);
	response.Format("%s: %s\n", trait_name, (trait_value == nullptr ? "" : trait_value));
	
	return CommandResult::OK;
}

static CommandResult
handle_set(Client &client, Request request, Response &response)
{
	if (request.size != 2) {
		response.FormatError(ACK_ERROR_ARG, "Wrong number of arguments: %s", ClientTraits::COMMAND_SYNTAX);
		return CommandResult::ERROR;
	}
	
	const char* trait_name = request.shift();
	if (trait_name == nullptr)
		trait_name = "";
	
	const char* trait_value = request.shift();
	if (trait_value == nullptr)
		trait_value = "";
	
	ClientTraits& traits = client.GetTraits();

	ClientTraits::Trait trait = traits.trait(trait_name);
	if (trait == ClientTraits::Trait::Invalid) {
		response.FormatError(ACK_ERROR_ARG, "Unknown trait name: \"%s\"", trait_name);
		return CommandResult::ERROR;
	}

	if (!traits.set_trait(trait, trait_value)) {
		response.FormatError(ACK_ERROR_ARG, "Invalid trait value: \"%s\"", trait_value);
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_client_traits(Client &client, Request request, Response &response)
{
	int argc = request.size;
	
	if (argc < 1) {
		response.FormatError(ACK_ERROR_ARG, "Too few arguments: %s", ClientTraits::COMMAND_SYNTAX);
		return CommandResult::ERROR;
	}

	if (argc > 3) {
		response.FormatError(ACK_ERROR_ARG, "Too many arguments: %s", ClientTraits::COMMAND_SYNTAX);
		return CommandResult::ERROR;
	}

	const char *const cmd = request.shift();
	if (StringIsEqual(cmd, "list")) {
		return handle_list(client, request, response);
	}

	if (StringIsEqual(cmd, "get")) {
		return handle_get(client, request, response);
	}

	if (StringIsEqual(cmd, "set")) {
		return handle_set(client, request, response);
	}

	response.FormatError(ACK_ERROR_ARG, "Invalid arguments: %s", ClientTraits::COMMAND_SYNTAX);
	return CommandResult::ERROR;
}
