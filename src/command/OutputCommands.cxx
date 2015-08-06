/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "OutputCommands.hxx"
#include "Request.hxx"
#include "output/OutputPrint.hxx"
#include "output/OutputCommand.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "util/ConstBuffer.hxx"

CommandResult
handle_enableoutput(Client &client, Request args)
{
	Response r(client);

	assert(args.size == 1);
	unsigned device;
	if (!args.Parse(0, device, r))
		return CommandResult::ERROR;

	if (!audio_output_enable_index(client.partition.outputs, device)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_disableoutput(Client &client, Request args)
{
	Response r(client);

	assert(args.size == 1);
	unsigned device;
	if (!args.Parse(0, device, r))
		return CommandResult::ERROR;

	if (!audio_output_disable_index(client.partition.outputs, device)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_toggleoutput(Client &client, Request args)
{
	Response r(client);

	assert(args.size == 1);
	unsigned device;
	if (!args.Parse(0, device, r))
		return CommandResult::ERROR;

	if (!audio_output_toggle_index(client.partition.outputs, device)) {
		r.Error(ACK_ERROR_NO_EXIST, "No such audio output");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_devices(Client &client, gcc_unused Request args)
{
	assert(args.IsEmpty());

	Response r(client);
	printAudioDevices(r, client.partition.outputs);

	return CommandResult::OK;
}
