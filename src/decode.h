/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include <stdio.h>
#include <sys/param.h>

#define DECODE_TYPE_MP3		0
#define DECODE_TYPE_OGG		1
#define DECODE_TYPE_FLAC	2
#define DECODE_TYPE_AUDIOFILE 	3
#define DECODE_TYPE_AAC 	4

#define DECODE_STATE_STOP	0
#define DECODE_STATE_DECODE	1

#define DECODE_ERROR_NOERROR	0
#define DECODE_ERROR_UNKTYPE	1

typedef struct _DecoderControl {
	int state;
	int stop;
	int start;
	int error;
	int seek;
	double seekWhere;
	char file[MAXPATHLEN+1];
} DecoderControl;

void decodeSigHandler(int sig);

void decode();

#endif
