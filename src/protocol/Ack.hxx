// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ACK_H
#define MPD_ACK_H

#include <stdexcept>
#include <utility>

enum ack {
	ACK_ERROR_NOT_LIST = 1,
	ACK_ERROR_ARG = 2,
	ACK_ERROR_PASSWORD = 3,
	ACK_ERROR_PERMISSION = 4,
	ACK_ERROR_UNKNOWN = 5,

	ACK_ERROR_NO_EXIST = 50,
	ACK_ERROR_PLAYLIST_MAX = 51,
	ACK_ERROR_SYSTEM = 52,
	ACK_ERROR_PLAYLIST_LOAD = 53,
	ACK_ERROR_UPDATE_ALREADY = 54,
	ACK_ERROR_PLAYER_SYNC = 55,
	ACK_ERROR_EXIST = 56,
};

class ProtocolError : public std::runtime_error {
	enum ack code;

public:
	template<typename M>
	ProtocolError(enum ack _code, M &&msg) noexcept
		:std::runtime_error(std::forward<M>(msg)), code(_code) {}

	enum ack GetCode() const noexcept {
		return code;
	}
};

#endif
