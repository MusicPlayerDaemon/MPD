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

#ifndef MPD_RESPONSE_HXX
#define MPD_RESPONSE_HXX

#include "protocol/Ack.hxx"

#include <fmt/core.h>
#if FMT_VERSION < 70000 || FMT_VERSION >= 80000
#include <fmt/format.h>
#endif

#include <cstddef>

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
	[[gnu::pure]]
	TagMask GetTagMask() const noexcept;

	void SetCommand(const char *_command) noexcept {
		command = _command;
	}

	bool Write(const void *data, size_t length) noexcept;
	bool Write(const char *data) noexcept;

	bool VFmt(fmt::string_view format_str, fmt::format_args args) noexcept;

	template<typename S, typename... Args>
	bool Fmt(const S &format_str, Args&&... args) noexcept {
#if FMT_VERSION >= 70000
		return VFmt(fmt::to_string_view(format_str),
			    fmt::make_args_checked<Args...>(format_str,
							    args...));
#else
		/* expensive fallback for older libfmt versions */
		const auto result = fmt::format(format_str, args...);
		return Write(result.data(), result.size());
#endif
	}

	/**
	 * Write a binary chunk; this writes the "binary" line, the
	 * given chunk and the trailing newline.
	 *
	 * @return true on success
	 */
	bool WriteBinary(ConstBuffer<void> payload) noexcept;

	void Error(enum ack code, const char *msg) noexcept;

	void VFmtError(enum ack code,
		       fmt::string_view format_str, fmt::format_args args) noexcept;

	template<typename S, typename... Args>
	void FmtError(enum ack code,
		      const S &format_str, Args&&... args) noexcept {
#if FMT_VERSION >= 70000
		return VFmtError(code, fmt::to_string_view(format_str),
				 fmt::make_args_checked<Args...>(format_str,
								 args...));
#else
		/* expensive fallback for older libfmt versions */
		const auto result = fmt::format(format_str, args...);
		return Error(code, result.c_str());
#endif
	}
};

#endif
