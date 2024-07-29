// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OutputCommands.hxx"
#include "Request.hxx"
#include "output/Print.hxx"
#include "output/OutputCommand.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "protocol/IdleFlags.hxx"
#include "util/CharUtil.hxx"
#include "util/StringVerify.hxx"

CommandResult
handle_enableoutput(Client &client, Request args, Response &r)
{
	assert(args.size() == 1);
	unsigned device = args.ParseUnsigned(0);

	auto &partition = client.GetPartition();

	if (!audio_output_enable_index(partition,
				       device)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_disableoutput(Client &client, Request args, Response &r)
{
	assert(args.size() == 1);
	unsigned device = args.ParseUnsigned(0);

	auto &partition = client.GetPartition();

	if (!audio_output_disable_index(partition,
					device)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_toggleoutput(Client &client, Request args, Response &r)
{
	assert(args.size() == 1);
	unsigned device = args.ParseUnsigned(0);

	auto &partition = client.GetPartition();

	if (!audio_output_toggle_index(partition,
				       device)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

static constexpr bool
IsValidAttributeNameChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) || ch == '_';
}

static constexpr bool
IsValidAttributeName(const char *s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidAttributeNameChar);
}

CommandResult
handle_outputset(Client &client, Request request, Response &response)
{
	assert(request.size() == 3);
	const unsigned i = request.ParseUnsigned(0);

	auto &partition = client.GetPartition();
	auto &outputs = partition.outputs;
	if (i >= outputs.Size()) {
		response.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	auto &ao = outputs.Get(i);

	const char *const name = request[1];
	if (!IsValidAttributeName(name)) {
		response.Error(ACK_ERROR_ARG, "Illegal attribute name");
		return CommandResult::ERROR;
	}

	const char *const value = request[2];

	ao.SetAttribute(name, value);

	partition.EmitIdle(IDLE_OUTPUT);

	return CommandResult::OK;
}

CommandResult
handle_devices(Client &client, [[maybe_unused]] Request args, Response &r)
{
	assert(args.empty());

	printAudioDevices(r, client.GetPartition().outputs);
	return CommandResult::OK;
}
