/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "OutputPrint.hxx"
#include "OutputCommand.hxx"
#include "protocol/Result.hxx"
#include "protocol/ArgParser.hxx"

#include <string.h>

enum command_return
handle_enableoutput(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned device;
	bool ret;

	if (!check_unsigned(client, &device, argv[1]))
		return COMMAND_RETURN_ERROR;

	ret = audio_output_enable_index(device);
	if (!ret) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "No such audio output");
		return COMMAND_RETURN_ERROR;
	}

	return COMMAND_RETURN_OK;
}

enum command_return
handle_disableoutput(Client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned device;
	bool ret;

	if (!check_unsigned(client, &device, argv[1]))
		return COMMAND_RETURN_ERROR;

	ret = audio_output_disable_index(device);
	if (!ret) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "No such audio output");
		return COMMAND_RETURN_ERROR;
	}

	return COMMAND_RETURN_OK;
}

enum command_return
handle_devices(Client *client,
	       G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	printAudioDevices(client);

	return COMMAND_RETURN_OK;
}
