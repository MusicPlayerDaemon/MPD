/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "AutoGunzipReader.hxx"
#include "GunzipReader.hxx"
#include "util/Error.hxx"

AutoGunzipReader::~AutoGunzipReader()
{
	delete gunzip;
}

gcc_pure
static bool
IsGzip(const uint8_t data[4])
{
	return data[0] == 0x1f && data[1] == 0x8b && data[2] == 0x08 &&
		(data[3] & 0xe0) == 0;
}

inline bool
AutoGunzipReader::Detect(Error &error)
{
	const uint8_t *data = (const uint8_t *)peek.Peek(4, error);
	if (data == nullptr) {
		if (error.IsDefined())
			return false;

		next = &peek;
		return true;
	}

	if (IsGzip(data)) {
		gunzip = new GunzipReader(peek, error);
		if (!gunzip->IsDefined())
			return false;


		next = gunzip;
	} else
		next = &peek;

	return true;
}

size_t
AutoGunzipReader::Read(void *data, size_t size, Error &error)
{
	if (next == nullptr && !Detect(error))
		return false;

	return next->Read(data, size, error);
}
