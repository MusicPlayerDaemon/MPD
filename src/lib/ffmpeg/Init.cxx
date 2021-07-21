/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
