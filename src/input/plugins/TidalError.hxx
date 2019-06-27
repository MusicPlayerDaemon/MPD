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

#ifndef TIDAL_ERROR_HXX
#define TIDAL_ERROR_HXX

#include <stdexcept>

/**
 * An error condition reported by the server.
 *
 * See http://developer.tidal.com/technical/errors/ for details (login
 * required).
 */
class TidalError : public std::runtime_error {
	/**
	 * The HTTP status code.
	 */
	unsigned status;

	/**
	 * The Tidal-specific "subStatus".  0 if none was found in the
	 * JSON response.
	 */
	unsigned sub_status;

public:
	template<typename W>
	TidalError(unsigned _status, unsigned _sub_status, W &&_what) noexcept
		:std::runtime_error(std::forward<W>(_what)),
		 status(_status), sub_status(_sub_status) {}

	unsigned GetStatus() const noexcept {
		return status;
	}

	unsigned GetSubStatus() const noexcept {
		return sub_status;
	}

	bool IsInvalidSession() const noexcept {
		return sub_status == 6001 || sub_status == 6002;
	}
};

#endif
