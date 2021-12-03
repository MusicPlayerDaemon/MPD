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

#ifndef MPD_INPUT_READER_HXX
#define MPD_INPUT_READER_HXX

#include "io/Reader.hxx"

class InputStream;

/**
 * A #Reader implementation which forwards all read calls to
 * InputStream::Read() and logs errors.
 */
class InputStreamReader final : public Reader {
	InputStream &is;

public:
	explicit InputStreamReader(InputStream &_is)
		:is(_is) {}

	/* virtual methods from class Reader */
	size_t Read(void *data, size_t size) override;
};

#endif
