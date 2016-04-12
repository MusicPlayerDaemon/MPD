/*
* MPD DVD-Audio Decoder plugin
* Copyright (c) 2014 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
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

#ifndef _DVD_INPUT_H_INCLUDED
#define _DVD_INPUT_H_INCLUDED

#include "dvda_media.h"

typedef dvda_media_t* dvd_input_t;

dvd_input_t dvdinput_open(dvd_input_t dev);
int         dvdinput_close(dvd_input_t dev);
int         dvdinput_seek(dvd_input_t dev, int block);
int         dvdinput_read(dvd_input_t dev, void *buffer, int blocks, int encrypted);

#endif
