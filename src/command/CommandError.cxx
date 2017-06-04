/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include <assert.h>

#define GLIBCXX_490 20140422
#define GLIBCXX_491 20140716
#define GLIBCXX_492 20141030
#define GLIBCXX_492_Debian_9 20141220
#define GLIBCXX_493 20150626
#define GLIBCXX_494 20160803
#define GLIBCXX_49X_NDK_r13b 20150123

/* the big mess attempts to detect whether we're compiling with
   libstdc++ 4.9.x; __GLIBCXX__ is a date tag and cannot be used to
   check the major version; and just checking the compiler version
   isn't enough, because somebody could use an old libstdc++ with
   clang - SIGH! */
#if GCC_OLDER_THAN(5,0) || (defined(__GLIBCXX__) &&		     \
	(__GLIBCXX__ == GLIBCXX_490 || __GLIBCXX__ == GLIBCXX_491 || \
	 __GLIBCXX__ == GLIBCXX_492 || \
	 __GLIBCXX__ == GLIBCXX_492_Debian_9 || \
	 __GLIBCXX__ == GLIBCXX_493 || \
	 __GLIBCXX__ == GLIBCXX_494 || \
	 __GLIBCXX__ == GLIBCXX_49X_NDK_r13b))
#define GLIBCXX_49X
#endif

gcc_const
static enum ack
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
gcc_const
static enum ack
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

gcc_pure
static enum ack
ToAck(std::exception_ptr ep) noexcept
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
#ifdef GLIBCXX_49X
	} catch (const std::exception &e) {
#else
	} catch (...) {
#endif
		try {
#ifdef GLIBCXX_49X
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
	LogError(ep);
	r.Error(ToAck(ep), FullMessage(ep).c_str());
}
