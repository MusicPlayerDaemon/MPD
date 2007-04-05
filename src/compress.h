/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 *
 * interface to audio compression
 * (c)2003-6 fluffy@beesbuzz.biz
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
 *
 */

#ifndef COMPRESS_H
#define COMPRESS_H

/* These are copied from the AudioCompress config.h, mainly because CompressDo
 * needs GAINSHIFT defined.  The rest are here so they can be used as defaults
 * to pass to CompressCfg(). -- jat */
#define ANTICLIP 0		/* Strict clipping protection */
#define TARGET 25000		/* Target level */
#define GAINMAX 32		/* The maximum amount to amplify by */
#define GAINSHIFT 10		/* How fine-grained the gain is */
#define GAINSMOOTH 8		/* How much inertia ramping has*/
#define BUCKETS 400		/* How long of a history to store */

void CompressCfg(int monitor,
		 int anticlip,
		 int target,
		 int maxgain,
		 int smooth,
		 int buckets);

void CompressDo(void *data, unsigned int numSamples);

void CompressFree(void);

#endif
