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

#include <system_error>

#include <assert.h>
#include <string.h>
#include <errno.h>

gcc_const
static enum ack
ToAck(PlaylistResult result)
{
	switch (result) {
	case PlaylistResult::SUCCESS:
		break;

	case PlaylistResult::DENIED:
		return ACK_ERROR_PERMISSION;

	case PlaylistResult::NO_SUCH_SONG:
	case PlaylistResult::NO_SUCH_LIST:
		return ACK_ERROR_NO_EXIST;

	case PlaylistResult::LIST_EXISTS:
		return ACK_ERROR_EXIST;

	case PlaylistResult::BAD_NAME:
	case PlaylistResult::BAD_RANGE:
		return ACK_ERROR_ARG;

	case PlaylistResult::NOT_PLAYING:
		return ACK_ERROR_PLAYER_SYNC;

	case PlaylistResult::TOO_LARGE:
		return ACK_ERROR_PLAYLIST_MAX;

	case PlaylistResult::DISABLED:
		break;
	}

	return ACK_ERROR_UNKNOWN;
}

CommandResult
print_playlist_result(Response &r, PlaylistResult result)
{
	switch (result) {
	case PlaylistResult::SUCCESS:
		return CommandResult::OK;

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

gcc_pure
static enum ack
ToAck(const Error &error)
{
	if (error.IsDomain(playlist_domain)) {
		return ToAck((PlaylistResult)error.GetCode());
	} else if (error.IsDomain(ack_domain)) {
		return (enum ack)error.GetCode();
#ifdef ENABLE_DATABASE
	} else if (error.IsDomain(db_domain)) {
		switch ((enum db_error)error.GetCode()) {
		case DB_DISABLED:
		case DB_NOT_FOUND:
			return ACK_ERROR_NO_EXIST;

		case DB_CONFLICT:
			return ACK_ERROR_ARG;
		}
#endif
	} else if (error.IsDomain(locate_uri_domain)) {
		return ACK_ERROR_ARG;
	} else if (error.IsDomain(errno_domain)) {
		return ACK_ERROR_SYSTEM;
	}

	return ACK_ERROR_UNKNOWN;
}

CommandResult
print_error(Response &r, const Error &error)
{
	assert(error.IsDefined());

	LogError(error);

	r.Error(ToAck(error), error.GetMessage());
	return CommandResult::ERROR;
}

void
PrintError(Response &r, std::exception_ptr ep)
{
	try {
		std::rethrow_exception(ep);
	} catch (const std::exception &e) {
		LogError(e);
	} catch (...) {
	}

	try {
		std::rethrow_exception(ep);
	} catch (const ProtocolError &pe) {
		r.Error(pe.GetCode(), pe.what());
	} catch (const PlaylistError &pe) {
		r.Error(ToAck(pe.GetCode()), pe.what());
	} catch (const std::system_error &e) {
		r.Error(ACK_ERROR_SYSTEM, e.what());
	} catch (const std::exception &e) {
		r.Error(ACK_ERROR_UNKNOWN, e.what());
	} catch (...) {
		r.Error(ACK_ERROR_UNKNOWN, "Unknown error");
	}
}
