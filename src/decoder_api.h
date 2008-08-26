/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef DECODER_API_H
#define DECODER_API_H

/*
 * This is the public API which is used by decoder plugins to
 * communicate with the mpd core.
 *
 */

#include "inputPlugin.h"
#include "inputStream.h"
#include "replayGain.h"

/**
 * Opaque handle which the decoder plugin passes to the functions in
 * this header.
 */
struct decoder;

/**
 * Notify the player thread that it has finished initialization and
 * that it has read the song's meta data.
 */
void decoder_initialized(struct decoder * decoder);

/**
 * This function is called by the decoder plugin when it has
 * successfully decoded block of input data.
 *
 * We send inStream for buffering the inputStream while waiting to
 * send the next chunk
 */
int decoder_data(struct decoder *decoder, InputStream * inStream,
		 int seekable,
		 void *data, size_t datalen,
		 float data_time, mpd_uint16 bitRate,
		 ReplayGainInfo * replayGainInfo);

#endif
