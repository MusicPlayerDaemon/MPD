// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FlacIOHandle.hxx"
#include "Log.hxx"
#include "system/Error.hxx"

#include <cerrno>

#include <stdio.h>

static size_t
FlacIORead(void *ptr, size_t size, size_t nmemb, FLAC__IOHandle handle)
{
	auto *is = (InputStream *)handle;

	std::byte *const p0 = (std::byte *)ptr, *p = p0,
		*const end = p0 + size * nmemb;

	/* libFLAC is very picky about short reads, and expects the IO
	   callback to fill the whole buffer (undocumented!) */

	while (p < end) {
		try {
			size_t nbytes = is->LockRead({p, end});
			if (nbytes == 0)
				/* end of file */
				break;

			p += nbytes;

#ifndef _WIN32
		} catch (const std::system_error &e) {
			errno = e.code().category() == ErrnoCategory()
				? e.code().value()
				/* just some random non-zero errno
				   value */
				: EINVAL;
			return 0;
#endif
		} catch (...) {
			/* just some random non-zero errno value */
			errno = EINVAL;
			return 0;
		}
	}

	/* libFLAC expects a clean errno after returning from the IO
	   callbacks (undocumented!) */
	errno = 0;
	return (p - p0) / size;
}

static int
FlacIOSeek(FLAC__IOHandle handle, FLAC__int64 _offset, int whence)
{
	auto *is = (InputStream *)handle;

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

	try {
		is->LockSeek(offset);
		return 0;
	} catch (...) {
		LogError(std::current_exception());
		return -1;
	}
}

static FLAC__int64
FlacIOTell(FLAC__IOHandle handle)
{
	auto *is = (InputStream *)handle;

	return is->GetOffset();
}

static int
FlacIOEof(FLAC__IOHandle handle)
{
	auto *is = (InputStream *)handle;

	return is->LockIsEOF();
}

static int
FlacIOClose([[maybe_unused]] FLAC__IOHandle handle)
{
	/* no-op because the libFLAC caller is responsible for closing
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
