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
#include "CommandError.hxx"
#include "db/DatabaseError.hxx"
#include "LocateUri.hxx"
#include "client/Response.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>
#include <errno.h>

CommandResult
print_playlist_result(Response &r, PlaylistResult result)
{
	switch (result) {
	case PlaylistResult::SUCCESS:
		return CommandResult::OK;

	case PlaylistResult::ERRNO:
		r.Error(ACK_ERROR_SYSTEM, strerror(errno));
		return CommandResult::ERROR;

	case PlaylistResult::DENIED:
		r.Error(ACK_ERROR_PERMISSION, "Access denied");
		return CommandResult::ERROR;

	case PlaylistResult::NO_SUCH_SONG:
		r.Error(ACK_ERROR_NO_EXIST, "No such song");
		return CommandResult::ERROR;

	case PlaylistResult::NO_SUCH_LIST:
		r.Error(ACK_ERROR_NO_EXIST, "No such playlist");
		return CommandResult::ERROR;

	case PlaylistResult::LIST_EXISTS:
		r.Error(ACK_ERROR_EXIST, "Playlist already exists");
		return CommandResult::ERROR;

	case PlaylistResult::BAD_NAME:
		r.Error(ACK_ERROR_ARG,
			"playlist name is invalid: "
			"playlist names may not contain slashes,"
			" newlines or carriage returns");
		return CommandResult::ERROR;

	case PlaylistResult::BAD_RANGE:
		r.Error(ACK_ERROR_ARG, "Bad song index");
		return CommandResult::ERROR;

	case PlaylistResult::NOT_PLAYING:
		r.Error(ACK_ERROR_PLAYER_SYNC, "Not playing");
		return CommandResult::ERROR;

	case PlaylistResult::TOO_LARGE:
		r.Error(ACK_ERROR_PLAYLIST_MAX,
			"playlist is at the max size");
		return CommandResult::ERROR;

	case PlaylistResult::DISABLED:
		r.Error(ACK_ERROR_UNKNOWN,
			"stored playlist support is disabled");
		return CommandResult::ERROR;
	}

	assert(0);
	return CommandResult::ERROR;
}

CommandResult
print_error(Response &r, const Error &error)
{
	assert(error.IsDefined());

	LogError(error);

	if (error.IsDomain(playlist_domain)) {
		return print_playlist_result(r,
					     PlaylistResult(error.GetCode()));
	} else if (error.IsDomain(ack_domain)) {
		r.Error((ack)error.GetCode(), error.GetMessage());
		return CommandResult::ERROR;
#ifdef ENABLE_DATABASE
	} else if (error.IsDomain(db_domain)) {
		switch ((enum db_error)error.GetCode()) {
		case DB_DISABLED:
			r.Error(ACK_ERROR_NO_EXIST, error.GetMessage());
			return CommandResult::ERROR;

		case DB_NOT_FOUND:
			r.Error(ACK_ERROR_NO_EXIST, "Not found");
			return CommandResult::ERROR;

		case DB_CONFLICT:
			r.Error(ACK_ERROR_ARG, "Conflict");
			return CommandResult::ERROR;
		}
#endif
	} else if (error.IsDomain(locate_uri_domain)) {
		r.Error(ACK_ERROR_ARG, error.GetMessage());
		return CommandResult::ERROR;
	} else if (error.IsDomain(errno_domain)) {
		r.Error(ACK_ERROR_SYSTEM, strerror(error.GetCode()));
		return CommandResult::ERROR;
	}

	r.Error(ACK_ERROR_UNKNOWN, "error");
	return CommandResult::ERROR;
}
