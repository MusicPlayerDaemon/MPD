/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "ParseInputStream.hxx"
#include "Handle.hxx"
#include "input/InputStream.hxx"

void
Yajl::ParseInputStream(Handle &handle, InputStream &is)
{
	const std::lock_guard<Mutex> protect(is.mutex);

	bool done = false;

	while (!done) {
		unsigned char buffer[4096];
		const size_t nbytes = is.Read(buffer, sizeof(buffer));
		if (nbytes == 0)
			done = true;

		if (done) {
			handle.CompleteParse();
		} else
			handle.Parse(buffer, nbytes);
	}
}
