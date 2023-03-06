// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Error.hxx"
#include "lib/fmt/RuntimeError.hxx"

extern "C" {
#include <libavutil/error.h>
}

std::runtime_error
MakeFfmpegError(int errnum)
{
	char msg[256];
	av_strerror(errnum, msg, sizeof(msg));
	return std::runtime_error(msg);
}

std::runtime_error
MakeFfmpegError(int errnum, const char *prefix)
{
	char msg[256];
	av_strerror(errnum, msg, sizeof(msg));
	return FmtRuntimeError("{}: {}", prefix, msg);
}
