// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_RESPONSE_HXX
#define MPD_RESPONSE_HXX

#include "protocol/Ack.hxx"

#include <fmt/core.h>
#if FMT_VERSION >= 80000 && FMT_VERSION < 90000
#include <fmt/format.h>
#endif

#include <cstddef>
#include <span>

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
#if FMT_VERSION >= 90000
		return VFmt(format_str,
			    fmt::make_format_args(args...));
#else
		return VFmt(fmt::to_string_view(format_str),
			    fmt::make_args_checked<Args...>(format_str,
							    args...));
#endif
	}

	/**
	 * Write a binary chunk; this writes the "binary" line, the
	 * given chunk and the trailing newline.
	 *
	 * @return true on success
	 */
	bool WriteBinary(std::span<const std::byte> payload) noexcept;

	void Error(enum ack code, const char *msg) noexcept;

	void VFmtError(enum ack code,
		       fmt::string_view format_str, fmt::format_args args) noexcept;

	template<typename S, typename... Args>
	void FmtError(enum ack code,
		      const S &format_str, Args&&... args) noexcept {
#if FMT_VERSION >= 90000
		return VFmtError(code, format_str,
				 fmt::make_format_args(args...));
#else
		return VFmtError(code, fmt::to_string_view(format_str),
				 fmt::make_args_checked<Args...>(format_str,
								 args...));
#endif
	}
};

#endif
