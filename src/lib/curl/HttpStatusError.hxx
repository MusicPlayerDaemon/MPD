// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <stdexcept>

/**
 * Thrown when an unsuccessful status was received from the HTTP
 * server.
 */
class HttpStatusError : public std::runtime_error {
	unsigned status;

public:
	template<typename M>
	explicit HttpStatusError(unsigned _status, M &&_msg) noexcept
		:std::runtime_error(std::forward<M>(_msg)), status(_status) {}

	unsigned GetStatus() const noexcept {
		return status;
	}
};
