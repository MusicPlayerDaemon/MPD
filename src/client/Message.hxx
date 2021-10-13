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

#ifndef MPD_CLIENT_MESSAGE_HXX
#define MPD_CLIENT_MESSAGE_HXX

#include <string>

#ifdef _WIN32
/* fuck WIN32! */
#include <windows.h>
#undef GetMessage
#endif

/**
 * A client-to-client message.
 */
class ClientMessage {
	std::string channel, message;

public:
	template<typename T, typename U>
	ClientMessage(T &&_channel, U &&_message)
		:channel(std::forward<T>(_channel)),
		 message(std::forward<U>(_message)) {}

	const char *GetChannel() const {
		return channel.c_str();
	}

	const char *GetMessage() const {
		return message.c_str();
	}
};

[[gnu::pure]]
bool
client_message_valid_channel_name(const char *name) noexcept;

#endif
