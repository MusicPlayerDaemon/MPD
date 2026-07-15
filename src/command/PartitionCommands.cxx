// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PartitionCommands.hxx"
#include "Request.hxx"
#include "Instance.hxx"
#include "Partition.hxx"
#include "protocol/IdleFlags.hxx"
#include "output/Filtered.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "util/CharUtil.hxx"
#include "util/StringVerify.hxx"

#include <fmt/format.h>

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
		r.Fmt("partition: {}\n", partition.name);
	}

	return CommandResult::OK;
}

static constexpr bool
IsValidPartitionChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) || ch == '-' || ch == '_';
}

static constexpr bool
IsValidPartitionName(const char *name) noexcept
{
	return CheckCharsNonEmpty(name, IsValidPartitionChar);
}

[[gnu::pure]]
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

	instance.partitions.emplace_back(name,
					 client.GetPartition());
	auto &partition = instance.partitions.back();
	partition.UpdateEffectiveReplayGainMode();

	instance.EmitIdle(IDLE_PARTITION);

	return CommandResult::OK;
}

CommandResult
handle_delpartition(Client &client, Request request, Response &response)
{
	const char *name = request.front();
	if (!IsValidPartitionName(name)) {
		response.Error(ACK_ERROR_ARG, "bad name");
		return CommandResult::ERROR;
	}

	auto &instance = client.GetInstance();
	auto *partition = instance.FindPartition(name);
	if (partition == nullptr) {
		response.Error(ACK_ERROR_NO_EXIST, "no such partition");
		return CommandResult::ERROR;
	}

	if (partition == &instance.partitions.front()) {
		response.Error(ACK_ERROR_UNKNOWN,
			       "cannot delete the default partition");
		return CommandResult::ERROR;
	}

	if (!partition->clients.empty()) {
		response.Error(ACK_ERROR_UNKNOWN,
			       "partition still has clients");
		return CommandResult::ERROR;
	}

	if (!partition->outputs.IsDummy()) {
		response.Error(ACK_ERROR_UNKNOWN,
			       "partition still has outputs");
		return CommandResult::ERROR;
	}

	partition->BeginShutdown();
	instance.DeletePartition(*partition);

	instance.EmitIdle(IDLE_PARTITION);

	return CommandResult::OK;
}

CommandResult
handle_moveoutput(Client &client, Request request, Response &response)
{
	const std::string_view output_name = request[0];

	auto &dest_partition = client.GetPartition();
	const auto existing_output_index = dest_partition.outputs.FindIndexByName(output_name);
	if (existing_output_index >= 0 && !dest_partition.outputs.Get(existing_output_index).IsDummy())
		/* this output is already in the specified partition,
		   so nothing needs to be done */
		return CommandResult::OK;

	/* find the partition which owns this output currently */
	auto &instance = client.GetInstance();

	const auto [src_partition, src_index] = instance.FindOutput(output_name, dest_partition);
	if (src_partition == nullptr) {
		response.Error(ACK_ERROR_NO_EXIST, "No such output");
		return CommandResult::ERROR;
	}

	const bool was_enabled = src_partition->outputs.Get(src_index).IsEnabled();

	auto output = src_partition->outputs.ReplaceWithDummy(src_index);

	if (existing_output_index >= 0)
		/* move the output back where it once was */
		dest_partition.outputs.ReplaceDummy(existing_output_index, std::move(output),
						    was_enabled,
						    dest_partition.replay_gain_mode);
	else
		/* copy the AudioOutputControl and add it to the output list */
		dest_partition.outputs.Add(std::move(output),
					   was_enabled,
					   dest_partition.replay_gain_mode);

	instance.EmitIdle(IDLE_OUTPUT);
	return CommandResult::OK;
}
