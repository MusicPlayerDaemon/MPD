// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_LOG_ERROR_HXX
#define MPD_FFMPEG_LOG_ERROR_HXX

void
LogFfmpegError(int errnum);

void
LogFfmpegError(int errnum, const char *prefix);

#endif
