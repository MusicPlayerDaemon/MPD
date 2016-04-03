/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "PlaylistError.hxx"
#include "db/DatabaseError.hxx"
#include "LocateUri.hxx"
#include "client/Response.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <system_error>

#include <assert.h>

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

#ifdef ENABLE_DATABASE
gcc_const
static enum ack
ToAck(DatabaseErrorCode code)
{
	switch (code) {
	case DatabaseErrorCode::DISABLED:
	case DatabaseErrorCode::NOT_FOUND:
		return ACK_ERROR_NO_EXIST;

	case DatabaseErrorCode::CONFLICT:
		return ACK_ERROR_ARG;
	}

	return ACK_ERROR_UNKNOWN;
}
#endif

gcc_pure
static enum ack
ToAck(const Error &error)
{
	if (error.IsDomain(ack_domain)) {
		return (enum ack)error.GetCode();
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

gcc_pure
static enum ack
ToAck(std::exception_ptr ep)
{
	try {
		std::rethrow_exception(ep);
	} catch (const ProtocolError &pe) {
		return pe.GetCode();
	} catch (const PlaylistError &pe) {
		return ToAck(pe.GetCode());
#ifdef ENABLE_DATABASE
	} catch (const DatabaseError &de) {
		return ToAck(de.GetCode());
#endif
	} catch (const std::system_error &e) {
		return ACK_ERROR_SYSTEM;
#if defined(__GLIBCXX__) && __GLIBCXX__ < 20151204
	} catch (const std::exception &e) {
#else
	} catch (...) {
#endif
		try {
#if defined(__GLIBCXX__) && __GLIBCXX__ < 20151204
			/* workaround for g++ 4.x: no overload for
			   rethrow_exception(exception_ptr) */
			std::rethrow_if_nested(e);
#else
			std::rethrow_if_nested(ep);
#endif
			return ACK_ERROR_UNKNOWN;
		} catch (...) {
			return ToAck(std::current_exception());
		}
	}
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
	} catch (const std::exception &e) {
		r.Error(ToAck(ep), e.what());
	} catch (...) {
		r.Error(ACK_ERROR_UNKNOWN, "Unknown error");
	}
}
