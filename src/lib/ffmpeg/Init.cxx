// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "Init.hxx"
#include "LogCallback.hxx"

extern "C" {
#include <libavutil/log.h>
}

void
FfmpegInit()
{
	av_log_set_callback(FfmpegLogCallback);
}
