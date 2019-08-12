/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_RESPONSE_HXX
#define MPD_RESPONSE_HXX

#include "protocol/Ack.hxx"
#include "util/Compiler.h"

#include <stddef.h>
#include <stdarg.h>

template<typename T> struct ConstBuffer;
class Client;
class TagMask;

class Response {
	Client &client;

	/**
	 * This command's index in the command list.  Used to generate
	 * error messages.
	 */
	const unsigned list_index;

	/**
	 * This command's name.  Used to generate error messages.
	 */
	const char *command = "";

public:
	Response(Client &_client, unsigned _list_index) noexcept
		:client(_client), list_index(_list_index) {}

	Response(const Response &) = delete;
	Response &operator=(const Response &) = delete;

	/**
	 * Returns a const reference to the associated #Client object.
	 * This should only be used to access a client's settings, to
	 * determine how to format the response.  For this reason, the
	 * returned reference is "const".
	 */
	const Client &GetClient() const noexcept {
		return client;
	}

	/**
	 * Accessor for Client::tag_mask.  Can be used if caller wants
	 * to avoid including Client.hxx.
	 */
	gcc_pure
	TagMask GetTagMask() const noexcept;

	void SetCommand(const char *_command) noexcept {
		command = _command;
	}

	bool Write(const void *data, size_t length) noexcept;
	bool Write(const char *data) noexcept;
	bool FormatV(const char *fmt, va_list args) noexcept;
	bool Format(const char *fmt, ...) noexcept;

	static constexpr size_t MAX_BINARY_SIZE = 8192;

	/**
	 * Write a binary chunk; this writes the "binary" line, the
	 * given chunk and the trailing newline.
	 *
	 * @return true on success
	 */
	bool WriteBinary(ConstBuffer<void> payload) noexcept;

	void Error(enum ack code, const char *msg) noexcept;
	void FormatError(enum ack code, const char *fmt, ...) noexcept;
};

#endif
