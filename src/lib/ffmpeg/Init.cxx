// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "Init.hxx"
#include "LogCallback.hxx"

extern "C" {
#include <libavformat/avformat.h>
}

void
FfmpegInit()
{
	av_log_set_callback(FfmpegLogCallback);

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	/* deprecated as of FFmpeg 4.0 */
	av_register_all();
#endif
}
