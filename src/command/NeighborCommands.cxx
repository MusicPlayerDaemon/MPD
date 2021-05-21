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

#include "NeighborCommands.hxx"
#include "Request.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Instance.hxx"
#include "neighbor/Glue.hxx"
#include "neighbor/Info.hxx"

#include <fmt/format.h>

#include <string>

bool
neighbor_commands_available(const Instance &instance) noexcept
{
	return instance.neighbors != nullptr;
}

CommandResult
handle_listneighbors(Client &client, [[maybe_unused]] Request args, Response &r)
{
	const NeighborGlue *const neighbors =
		client.GetInstance().neighbors.get();
	if (neighbors == nullptr) {
		r.Error(ACK_ERROR_UNKNOWN, "No neighbor plugin configured");
		return CommandResult::ERROR;
	}

	for (const auto &i : neighbors->GetList())
		r.Fmt(FMT_STRING("neighbor: {}\n"
				 "name: {}\n"),
		      i.uri,
		      i.display_name);
	return CommandResult::OK;
}
