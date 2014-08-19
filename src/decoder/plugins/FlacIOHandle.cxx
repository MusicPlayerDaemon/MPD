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
#include "FlacIOHandle.hxx"
#include "util/Error.hxx"
#include "Compiler.h"

#include <errno.h>
#include <stdio.h>

static size_t
FlacIORead(void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle)
{
	InputStream *is = (InputStream *)handle;

	uint8_t *const p0 = (uint8_t *)ptr, *p = p0,
		*const end = p0 + size * nmemb;

	/* libFLAC is very picky about short reads, and expects the IO
	   callback to fill the whole buffer (undocumented!) */

	Error error;
	while (p < end) {
		size_t nbytes = is->LockRead(p, end - p, error);
		if (nbytes == 0) {
			if (!error.IsDefined())
				/* end of file */
				break;

			if (error.IsDomain(errno_domain))
				errno = error.GetCode();
			else
				/* just some random non-zero
				   errno value */
				errno = EINVAL;
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
FlacIOSeek(FLAC__IOHandle handle, FLAC__int64 _offset, int whence)
{
	InputStream *is = (InputStream *)handle;

	offset_type offset = _offset;
	switch (whence) {
	case SEEK_SET:
		break;

	case SEEK_CUR:
		offset += is->GetOffset();
		break;

	case SEEK_END:
		if (!is->KnownSize())
			return -1;

		offset += is->GetSize();
		break;

	default:
		return -1;
	}

	return is->LockSeek(offset, IgnoreError()) ? 0 : -1;
}

static FLAC__int64
FlacIOTell(FLAC__IOHandle handle)
{
	InputStream *is = (InputStream *)handle;

	return is->GetOffset();
}

static int
FlacIOEof(FLAC__IOHandle handle)
{
	InputStream *is = (InputStream *)handle;

	return is->LockIsEOF();
}

static int
FlacIOClose(gcc_unused FLAC__IOHandle handle)
{
	/* no-op because the libFLAC caller is repsonsible for closing
	   the #InputStream */

	return 0;
}

const FLAC__IOCallbacks flac_io_callbacks = {
	FlacIORead,
	nullptr,
	nullptr,
	nullptr,
	FlacIOEof,
	FlacIOClose,
};

const FLAC__IOCallbacks flac_io_callbacks_seekable = {
	FlacIORead,
	nullptr,
	FlacIOSeek,
	FlacIOTell,
	FlacIOEof,
	FlacIOClose,
};
