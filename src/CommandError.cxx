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
#include "db_error.h"
#include "io_error.h"
#include "protocol/Result.hxx"

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

/**
 * Send the GError to the client and free the GError.
 */
enum command_return
print_error(Client *client, GError *error)
{
	assert(client != NULL);
	assert(error != NULL);

	g_warning("%s", error->message);

	if (error->domain == playlist_quark()) {
		enum playlist_result result = (playlist_result)error->code;
		g_error_free(error);
		return print_playlist_result(client, result);
	} else if (error->domain == ack_quark()) {
		command_error(client, (ack)error->code, "%s", error->message);
		g_error_free(error);
		return COMMAND_RETURN_ERROR;
	} else if (error->domain == db_quark()) {
		switch ((enum db_error)error->code) {
		case DB_DISABLED:
			command_error(client, ACK_ERROR_NO_EXIST, "%s",
				      error->message);
			g_error_free(error);
			return COMMAND_RETURN_ERROR;

		case DB_NOT_FOUND:
			g_error_free(error);
			command_error(client, ACK_ERROR_NO_EXIST, "Not found");
			return COMMAND_RETURN_ERROR;
		}
	} else if (error->domain == errno_quark()) {
		command_error(client, ACK_ERROR_SYSTEM, "%s",
			      g_strerror(error->code));
		g_error_free(error);
		return COMMAND_RETURN_ERROR;
	} else if (error->domain == g_file_error_quark()) {
		command_error(client, ACK_ERROR_SYSTEM, "%s", error->message);
		g_error_free(error);
		return COMMAND_RETURN_ERROR;
	}

	g_error_free(error);
	command_error(client, ACK_ERROR_UNKNOWN, "error");
	return COMMAND_RETURN_ERROR;
}
