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
#include "CommandError.hxx"
#include "DatabaseError.hxx"
#include "protocol/Result.hxx"
#include "util/Error.hxx"

#include <glib.h>

#include <assert.h>
#include <errno.h>

enum command_return
print_playlist_result(Client *client, enum playlist_result result)
{
	switch (result) {
	case PLAYLIST_RESULT_SUCCESS:
		return COMMAND_RETURN_OK;

	case PLAYLIST_RESULT_ERRNO:
		command_error(client, ACK_ERROR_SYSTEM, "%s",
			      g_strerror(errno));
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_DENIED:
		command_error(client, ACK_ERROR_PERMISSION, "Access denied");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_NO_SUCH_SONG:
		command_error(client, ACK_ERROR_NO_EXIST, "No such song");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_NO_SUCH_LIST:
		command_error(client, ACK_ERROR_NO_EXIST, "No such playlist");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_LIST_EXISTS:
		command_error(client, ACK_ERROR_EXIST,
			      "Playlist already exists");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_BAD_NAME:
		command_error(client, ACK_ERROR_ARG,
			      "playlist name is invalid: "
			      "playlist names may not contain slashes,"
			      " newlines or carriage returns");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_BAD_RANGE:
		command_error(client, ACK_ERROR_ARG, "Bad song index");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_NOT_PLAYING:
		command_error(client, ACK_ERROR_PLAYER_SYNC, "Not playing");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_TOO_LARGE:
		command_error(client, ACK_ERROR_PLAYLIST_MAX,
			      "playlist is at the max size");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_DISABLED:
		command_error(client, ACK_ERROR_UNKNOWN,
			      "stored playlist support is disabled");
		return COMMAND_RETURN_ERROR;
	}

	assert(0);
	return COMMAND_RETURN_ERROR;
}

enum command_return
print_error(Client *client, const Error &error)
{
	assert(client != NULL);
	assert(error.IsDefined());

	g_warning("%s", error.GetMessage());

	if (error.IsDomain(playlist_domain)) {
		return print_playlist_result(client,
					     playlist_result(error.GetCode()));
	} else if (error.IsDomain(ack_domain)) {
		command_error(client, (ack)error.GetCode(),
			      "%s", error.GetMessage());
		return COMMAND_RETURN_ERROR;
	} else if (error.IsDomain(db_domain)) {
		switch ((enum db_error)error.GetCode()) {
		case DB_DISABLED:
			command_error(client, ACK_ERROR_NO_EXIST, "%s",
				      error.GetMessage());
			return COMMAND_RETURN_ERROR;

		case DB_NOT_FOUND:
			command_error(client, ACK_ERROR_NO_EXIST, "Not found");
			return COMMAND_RETURN_ERROR;
		}
	} else if (error.IsDomain(errno_domain)) {
		command_error(client, ACK_ERROR_SYSTEM, "%s",
			      g_strerror(error.GetCode()));
		return COMMAND_RETURN_ERROR;
	}

	command_error(client, ACK_ERROR_UNKNOWN, "error");
	return COMMAND_RETURN_ERROR;
}
