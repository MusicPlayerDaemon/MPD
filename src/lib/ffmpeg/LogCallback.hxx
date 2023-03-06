// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_LOG_CALLBACK_HXX
#define MPD_FFMPEG_LOG_CALLBACK_HXX

#include <cstdarg>

void
FfmpegLogCallback(void *ptr, int level, const char *fmt, std::va_list vl);

#endif
