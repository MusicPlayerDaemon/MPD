/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "FLACIOHandle.hxx"
#include "io_error.h"
#include "gcc.h"

#include <errno.h>

static size_t
FLACIORead(void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle)
{
	input_stream *is = (input_stream *)handle;

	uint8_t *const p0 = (uint8_t *)ptr, *p = p0,
		*const end = p0 + size * nmemb;

	/* libFLAC is very picky about short reads, and expects the IO
	   callback to fill the whole buffer (undocumented!) */

	GError *error = nullptr;
	while (p < end) {
		size_t nbytes = input_stream_lock_read(is, p, end - p, &error);
		if (nbytes == 0) {
			if (error == nullptr)
				/* end of file */
				break;

			if (error->domain == errno_quark())
				errno = error->code;
			else
				/* just some random non-zero
				   errno value */
				errno = EINVAL;
			g_error_free(error);
			return 0;
		}

		p += nbytes;
	}

	/* libFLAC expects a clean errno after returning from the IO
	   callbacks (undocumented!) */
	errno = 0;
	return (p - p0) / size;
}

static int
FLACIOSeek(FLAC__IOHandle handle, FLAC__int64 offset, int whence)
{
	input_stream *is = (input_stream *)handle;

	return input_stream_lock_seek(is, offset, whence, nullptr) ? 0 : -1;
}

static FLAC__int64
FLACIOTell(FLAC__IOHandle handle)
{
	input_stream *is = (input_stream *)handle;

	return is->offset;
}

static int
FLACIOEof(FLAC__IOHandle handle)
{
	input_stream *is = (input_stream *)handle;

	return input_stream_lock_eof(is);
}

static int
FLACIOClose(gcc_unused FLAC__IOHandle handle)
{
	/* no-op because the libFLAC caller is repsonsible for closing
	   the #input_stream */

	return 0;
}

const FLAC__IOCallbacks flac_io_callbacks = {
	FLACIORead,
	nullptr,
	nullptr,
	nullptr,
	FLACIOEof,
	FLACIOClose,
};

const FLAC__IOCallbacks flac_io_callbacks_seekable = {
	FLACIORead,
	nullptr,
	FLACIOSeek,
	FLACIOTell,
	FLACIOEof,
	FLACIOClose,
};
