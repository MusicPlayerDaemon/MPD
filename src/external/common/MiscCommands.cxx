/*
 * Copyright 2015-2018 Cary Audio
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

#include "config.h"
#include "MiscCommands.hxx"
#include "command/Request.hxx"
#include "command/CommandError.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "external/common/Context.hxx"
#include "util/StringAPI.hxx"
#include "protocol/Ack.hxx"

static CommandResult
handle_tpm_tidal_session(Client &client, Request args, Response &r)
{
	auto &context = client.GetContext();

	if (StringIsEqual(args[0], "session")) {
		if (args.size == 1) { // get
			r.Format("audioquality: %s\n", context.tidal.audioquality.c_str());
			r.Format("sessionId: %s\n", context.tidal.sessionId.c_str());
			return CommandResult::OK;
		}

		jaijson::Document doc;
		if (doc.Parse(args[1]).HasParseError()) {
			throw FormatProtocolError(ACK_ERROR_ARG, "parse json %s fail", args[1]);
		}
		deserialize(doc, context.tidal);
		return CommandResult::OK;
	}

	throw FormatProtocolError(ACK_ERROR_ARG, "unkown config(%s)", args[0]);
}

static CommandResult
handle_tpm_qobuz_session(Client &client, Request args, Response &r)
{
	auto &context = client.GetContext();

	if (StringIsEqual(args[0], "session")) {
		if (args.size == 1) { // get
			r.Format("format_id: %d\n", context.qobuz.format_id);
			r.Format("user_auth_token: %s\n", context.qobuz.user_auth_token.c_str());
			return CommandResult::OK;
		}

		jaijson::Document doc;
		if (doc.Parse(args[1]).HasParseError()) {
			throw FormatProtocolError(ACK_ERROR_ARG, "parse json %s fail", args[1]);
		}
		deserialize(doc, context.qobuz);
		return CommandResult::OK;
	}

	throw FormatProtocolError(ACK_ERROR_ARG, "unkown config(%s)", args[0]);
}

CommandResult
handle_tpm_commands(Client &client, Request args, Response &r)
{
	if (StringIsEqual(args[0], "tidal")) {
		args.pop_front();
		return handle_tpm_tidal_session(client, args, r);
	} else if (StringIsEqual(args[0], "qobuz")) {
		args.pop_front();
		return handle_tpm_qobuz_session(client, args, r);
	} else {
		throw FormatProtocolError(ACK_ERROR_ARG, "unkown domain(%s)", args[0]);
	}
}

