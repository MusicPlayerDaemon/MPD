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

#include "OutputCommands.hxx"
#include "Request.hxx"
#include "output/Print.hxx"
#include "output/OutputCommand.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "IdleFlags.hxx"
#include "util/CharUtil.hxx"

CommandResult
handle_enableoutput(Client &client, Request args, Response &r)
{
	assert(args.size == 1);
	unsigned device = args.ParseUnsigned(0);

	if (!audio_output_enable_index(client.GetPartition().outputs, device)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_disableoutput(Client &client, Request args, Response &r)
{
	assert(args.size == 1);
	unsigned device = args.ParseUnsigned(0);

	if (!audio_output_disable_index(client.GetPartition().outputs, device)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_toggleoutput(Client &client, Request args, Response &r)
{
	assert(args.size == 1);
	unsigned device = args.ParseUnsigned(0);

	if (!audio_output_toggle_index(client.GetPartition().outputs, device)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

static bool
IsValidAttributeNameChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) || ch == '_';
}

gcc_pure
static bool
IsValidAttributeName(const char *s) noexcept
{
	do {
		if (!IsValidAttributeNameChar(*s))
			return false;
	} while (*++s);

	return true;
}

CommandResult
handle_outputset(Client &client, Request request, Response &response)
{
	assert(request.size == 3);
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
