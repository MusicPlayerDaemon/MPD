/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "Reader.hxx"
#include "InputStream.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

size_t
InputStreamReader::Read(void *data, size_t size)
{
	Error error;
	size_t nbytes = is.LockRead(data, size, error);
	assert(nbytes == 0 || !error.IsDefined());
	assert(nbytes > 0 || error.IsDefined() || is.IsEOF());

	if (gcc_unlikely(nbytes == 0 && error.IsDefined()))
		LogError(error);

	return nbytes;
}
