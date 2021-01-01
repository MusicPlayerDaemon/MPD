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

#include "Error.hxx"
#include "util/RuntimeError.hxx"

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
	return FormatRuntimeError("%s: %s", prefix, msg);
}
