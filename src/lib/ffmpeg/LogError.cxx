// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LogError.hxx"
#include "Domain.hxx"
#include "Log.hxx"

extern "C" {
#include <libavutil/error.h>
}

void
LogFfmpegError(int errnum)
{
	char msg[256];
	av_strerror(errnum, msg, sizeof(msg));
	LogError(ffmpeg_domain, msg);
}

void
LogFfmpegError(int errnum, const char *prefix)
{
	char msg[256];
	av_strerror(errnum, msg, sizeof(msg));
	FmtError(ffmpeg_domain, "{}: {}", prefix, msg);
}
