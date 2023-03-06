// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
