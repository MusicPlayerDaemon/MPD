/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu
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

#ifndef TAG_H
#define TAG_H

#include "../config.h"

#include <stdio.h>
#ifdef HAVE_ID3TAG
#ifdef USE_MPD_ID3TAG
#include "libid3tag/id3tag.h"
#else
#include <id3tag.h>
#endif
#endif

typedef struct _MpdTag {
	char * artist;
	char * album;
	char * track;
	char * title;
	char * name;
	int time;
} MpdTag;

#ifdef HAVE_ID3TAG
MpdTag * parseId3Tag(struct id3_tag *);
#endif

MpdTag * id3Dup(char * file);

MpdTag * newMpdTag();

void clearMpdTag(MpdTag * tag);

void freeMpdTag(MpdTag * tag);

void printMpdTag(FILE * fp, MpdTag * tag);

MpdTag * mpdTagDup(MpdTag * tag);

void validateUtf8Tag(MpdTag * tag);

int mpdTagsAreEqual(MpdTag * tag1, MpdTag * tag2);

#endif
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
