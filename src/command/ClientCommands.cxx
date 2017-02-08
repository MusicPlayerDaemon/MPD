/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "ClientCommands.hxx"
#include "Request.hxx"
#include "Permission.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "TagPrint.hxx"

CommandResult
handle_close(gcc_unused Client &client, gcc_unused Request args,
	     gcc_unused Response &r)
{
	return CommandResult::FINISH;
}

CommandResult
handle_ping(gcc_unused Client &client, gcc_unused Request args,
	    gcc_unused Response &r)
{
	return CommandResult::OK;
}

CommandResult
handle_password(Client &client, Request args, Response &r)
{
	unsigned permission = 0;
	if (getPermissionFromPassword(args.front(), &permission) < 0) {
		r.Error(ACK_ERROR_PASSWORD, "incorrect password");
		return CommandResult::ERROR;
	}

	client.SetPermission(permission);

	return CommandResult::OK;
}

CommandResult
handle_tagtypes(gcc_unused Client &client, gcc_unused Request request,
		Response &r)
{
	tag_print_types(r);
	return CommandResult::OK;
}
