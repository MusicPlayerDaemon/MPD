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

#ifndef CURL_ERROR_HXX
#define CURL_ERROR_HXX

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

#endif
