// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FLAC_IO_HANDLE_HXX
#define MPD_FLAC_IO_HANDLE_HXX

#include "input/InputStream.hxx"

#include <FLAC/callback.h>

extern const FLAC__IOCallbacks flac_io_callbacks;
extern const FLAC__IOCallbacks flac_io_callbacks_seekable;

static inline FLAC__IOHandle
ToFlacIOHandle(InputStream &is)
{
	return (FLAC__IOHandle)&is;
}

static inline const FLAC__IOCallbacks &
GetFlacIOCallbacks(const InputStream &is)
{
	return is.IsSeekable()
		? flac_io_callbacks_seekable
		: flac_io_callbacks;
}

#endif
