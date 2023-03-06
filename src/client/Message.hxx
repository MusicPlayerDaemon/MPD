// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
