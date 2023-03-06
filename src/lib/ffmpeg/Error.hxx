// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_ERROR_HXX
#define MPD_FFMPEG_ERROR_HXX

#include <stdexcept>

std::runtime_error
MakeFfmpegError(int errnum);

std::runtime_error
MakeFfmpegError(int errnum, const char *prefix);

#endif
