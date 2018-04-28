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
#include "NeighborCommands.hxx"
#include "Request.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Instance.hxx"
#include "neighbor/Glue.hxx"
#include "neighbor/Info.hxx"
#include "util/StringCompare.hxx"
#include "db/Interface.hxx"
#include "Partition.hxx"
#include "db/plugins/upnp/UpnpDatabasePlugin.hxx"

#include <string>

bool
neighbor_commands_available(const Instance &instance) noexcept
{
	return instance.neighbors != nullptr;
}

CommandResult
handle_listneighbors(Client &client, gcc_unused Request args, Response &r)
{
	const NeighborGlue *const neighbors =
		client.GetInstance().neighbors;
	if (neighbors == nullptr) {
		r.Error(ACK_ERROR_UNKNOWN, "No neighbor plugin configured");
		return CommandResult::ERROR;
	}

	for (const auto &i : neighbors->GetList())
		r.Format("neighbor: %s\n"
			 "name: %s\n",
			 i.uri.c_str(),
			 i.display_name.c_str());
	return CommandResult::OK;
}

CommandResult
handle_scanNeighbors(Client &client, gcc_unused Request args, Response &r)
{
	Database *upnpdatabase = client.GetPartition().instance.upnpdatabase;
	NeighborGlue *neighbors =
		client.GetPartition().instance.neighbors;
	if (neighbors == nullptr) {
		r.Error(ACK_ERROR_UNKNOWN,
			      "No neighbor plugin configured");
		return CommandResult::ERROR;
	}

	auto state = client.GetPlayerControl().GetState();
	if (state == PlayerState::PLAY) {
		client.GetPlayerControl().LockPause();
	}

	if (upnpdatabase != nullptr) {
		upnpdatabase->Close();
	}

	try {
		neighbors->Reopen();
	} catch (...) {
		r.Error(ACK_ERROR_SYSTEM,
				  "reopen neighbors error");
		return CommandResult::ERROR;
	}

	if (upnpdatabase != nullptr) {
		try {
			upnpdatabase->Open();
		} catch (...) {
			r.Error(ACK_ERROR_SYSTEM,
					  "open upnp error");
			return CommandResult::ERROR;
		}
	}

	return CommandResult::OK;
}

CommandResult
handle_clear_upnp_cache(gcc_unused Client &client, gcc_unused Request args, gcc_unused Response &r)
{
	ClearUpnpCache();

	return CommandResult::OK;
}
