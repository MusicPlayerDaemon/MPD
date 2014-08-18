/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "Error.hxx"
#include "Domain.hxx"
#include "util/Error.hxx"

extern "C" {
#include <libavutil/error.h>
}

void
SetFfmpegError(Error &error, int errnum)
{
	char msg[256];
	av_strerror(errnum, msg, sizeof(msg));
	error.Set(ffmpeg_domain, errnum, msg);
}

void
SetFfmpegError(Error &error, int errnum, const char *prefix)
{
	char msg[256];
	av_strerror(errnum, msg, sizeof(msg));
	error.Format(ffmpeg_domain, errnum, "%s: %s", prefix, msg);
}
