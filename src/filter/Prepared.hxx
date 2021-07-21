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

#ifndef MPD_PREPARED_FILTER_HXX
#define MPD_PREPARED_FILTER_HXX

#include <memory>

struct AudioFormat;
class Filter;

class PreparedFilter {
public:
	virtual ~PreparedFilter() = default;

	/**
	 * Opens the filter, preparing it for FilterPCM().
	 *
	 * Throws on error.
	 *
	 * @param af the audio format of incoming data; the
	 * plugin may modify the object to enforce another input
	 * format
	 */
	virtual std::unique_ptr<Filter> Open(AudioFormat &af) = 0;
};

#endif
