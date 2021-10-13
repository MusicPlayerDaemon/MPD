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

#include "config.h"
#include "CommandError.hxx"
#include "PlaylistError.hxx"
#include "db/DatabaseError.hxx"
#include "client/Response.hxx"
#include "Log.hxx"
#include "util/Exception.hxx"

#include <system_error>

static constexpr enum ack
ToAck(PlaylistResult result) noexcept
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

static constexpr enum ack
ToAck(DatabaseErrorCode code) noexcept
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

[[gnu::pure]]
static enum ack
ToAck(const std::exception_ptr& ep) noexcept
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
	} catch (const std::invalid_argument &e) {
		return ACK_ERROR_ARG;
	} catch (const std::length_error &e) {
		return ACK_ERROR_ARG;
	} catch (const std::out_of_range &e) {
		return ACK_ERROR_ARG;
	} catch (...) {
		try {
			std::rethrow_if_nested(ep);
			return ACK_ERROR_UNKNOWN;
		} catch (...) {
			return ToAck(std::current_exception());
		}
	}
}

void
PrintError(Response &r, const std::exception_ptr& ep)
{
	LogError(ep);
	r.Error(ToAck(ep), GetFullMessage(ep).c_str());
}
