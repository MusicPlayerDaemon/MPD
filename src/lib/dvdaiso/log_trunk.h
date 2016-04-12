/*
* MPD DVD-Audio Decoder plugin
* Copyright (c) 2016 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* DVD-Audio Decoder is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* DVD-Audio Decoder is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _LOG_TRUNK
#define _LOG_TRUNK

#include <stdarg.h>

void mpd_av_log_callback(void* ptr, int level, const char* fmt, va_list vl);
void mpd_dprintf(void* ptr, const char* fmt, ...);

void my_av_log_set_callback(void (*callback)(void* ptr, int, const char* fmt, va_list vl));
void my_av_log_set_default_callback();

#endif
