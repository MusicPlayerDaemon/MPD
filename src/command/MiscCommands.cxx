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
#include "Request.hxx"
#include "CommandError.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "util/StringAPI.hxx"
#include "protocol/Ack.hxx"
#include "input/plugins/QobuzInputPlugin.hxx"
#include "input/plugins/QobuzSession.hxx"
#include "input/plugins/TidalInputPlugin.hxx"
#include "input/plugins/TidalSessionManager.hxx"

#include "external/jaijson/Deserializer.hxx"

static bool
deserialize(const jaijson::Value &d, TidalSessionManager &m)
{
	std::string session;
	std::string audioquality;

	deserialize(d, "sessionId", session);
	deserialize(d, "audioquality", audioquality);
	m.SetSession(session);
	m.SetQudioQuality(audioquality);

	return true;
}

static CommandResult
handle_tpm_tidal_session(gcc_unused Client &client, Request args, Response &r)
{
	auto &tidal = GetTidalSession();

	if (StringIsEqual(args[0], "session")) {
		if (args.size == 1) { // get
			r.Format("audioquality: %s\n", tidal.GetQudioQuality().c_str());
			r.Format("sessionId: %s\n", tidal.GetSession().c_str());
			return CommandResult::OK;
		} else {
			deserialize(args[1], tidal);
			return CommandResult::OK;
		}
	}

	throw FormatProtocolError(ACK_ERROR_ARG, "unkown config(%s)", args[0]);
}

static CommandResult
handle_tpm_qobuz_session(gcc_unused Client &client, Request args, Response &r)
{
	auto &qobuz = GetQobuzSession();

	if (StringIsEqual(args[0], "session")) {
		if (args.size == 1) { // get
			r.Format("format_id: %d\n", qobuz.format_id);
			r.Format("user_auth_token: %s\n", qobuz.user_auth_token.c_str());
			return CommandResult::OK;
		}

		deserialize(args[1], qobuz);
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

