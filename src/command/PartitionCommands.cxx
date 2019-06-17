/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "PartitionCommands.hxx"
#include "Request.hxx"
#include "Instance.hxx"
#include "Partition.hxx"
#include "IdleFlags.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "util/CharUtil.hxx"

CommandResult
handle_partition(Client &client, Request request, Response &response)
{
	const char *name = request.front();
	auto &instance = client.GetInstance();
	auto *partition = instance.FindPartition(name);
	if (partition == nullptr) {
		response.Error(ACK_ERROR_NO_EXIST, "partition does not exist");
		return CommandResult::ERROR;
	}

	client.SetPartition(*partition);
	return CommandResult::OK;
}

CommandResult
handle_listpartitions(Client &client, Request, Response &r)
{
	for (const auto &partition : client.GetInstance().partitions) {
		r.Format("partition: %s\n", partition.name.c_str());
	}

	return CommandResult::OK;
}

static constexpr bool
IsValidPartitionChar(char ch)
{
	return IsAlphaNumericASCII(ch) || ch == '-' || ch == '_';
}

gcc_pure
static bool
IsValidPartitionName(const char *name) noexcept
{
	do {
		if (!IsValidPartitionChar(*name))
			return false;
	} while (*++name != 0);

	return true;
}

gcc_pure
static bool
HasPartitionNamed(Instance &instance, const char *name) noexcept
{
	return instance.FindPartition(name) != nullptr;
}

CommandResult
handle_newpartition(Client &client, Request request, Response &response)
{
	const char *name = request.front();
	if (!IsValidPartitionName(name)) {
		response.Error(ACK_ERROR_ARG, "bad name");
		return CommandResult::ERROR;
	}

	auto &instance = client.GetInstance();
	if (HasPartitionNamed(instance, name)) {
		response.Error(ACK_ERROR_EXIST, "name already exists");
		return CommandResult::ERROR;
	}

	if (instance.partitions.size() >= 16) {
		/* arbitrary limit for now */
		response.Error(ACK_ERROR_UNKNOWN, "too many partitions");
		return CommandResult::ERROR;
	}

	instance.partitions.emplace_back(instance, name,
					 // TODO: use real configuration
					 16384,
					 1024,
					 AudioFormat::Undefined(),
					 ReplayGainConfig());
	auto &partition = instance.partitions.back();
	partition.outputs.AddNullOutput(instance.io_thread.GetEventLoop(),
					ReplayGainConfig(),
					partition.pc);
	partition.UpdateEffectiveReplayGainMode();

	instance.EmitIdle(IDLE_PARTITION);

	return CommandResult::OK;
}
