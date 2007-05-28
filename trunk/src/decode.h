/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#ifndef DECODE_H
#define DECODE_H

#include "../config.h"
#include "tag.h"

#include "mpd_types.h"
#include "audio.h"

#include <stdio.h>
#include <sys/param.h>
#include <signal.h>

#define DECODE_TYPE_FILE	0
#define DECODE_TYPE_URL		1

#define DECODE_STATE_STOP	0
#define DECODE_STATE_START      1
#define DECODE_STATE_DECODE	2

#define DECODE_ERROR_NOERROR	0
#define DECODE_ERROR_UNKTYPE	10
#define DECODE_ERROR_FILE	20

#define DECODE_SUFFIX_MP3       1
#define DECODE_SUFFIX_OGG       2
#define DECODE_SUFFIX_FLAC      3
#define DECODE_SUFFIX_AAC       4
#define DECODE_SUFFIX_MP4       5
#define DECODE_SUFFIX_WAVE      6

typedef struct _DecoderControl {
	volatile mpd_sint8 state;
	volatile mpd_sint8 stop;
	volatile mpd_sint8 start;
	volatile mpd_uint16 error;
	volatile mpd_sint8 seek;
	volatile mpd_sint8 seekError;
	volatile mpd_sint8 seekable;
	volatile mpd_sint8 cycleLogFiles;
	volatile double seekWhere;
	AudioFormat audioFormat;
	char utf8url[MAXPATHLEN + 1];
	volatile float totalTime;
} DecoderControl;

void decodeSigHandler(int sig, siginfo_t * siginfo, void *v);

void decode(void);

#endif
