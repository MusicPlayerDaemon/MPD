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

#ifndef INPUT_STREAM_H
#define INPUT_STREAM_H

#include <stdio.h>
#include <stdlib.h>

typedef struct _InputStream {
	FILE * fp;
	int error;
	long offset;
	size_t size;
} InputStream;

/* if an error occurs for these 3 functions, then -1 is returned and errno
   for the input stream is set */
int initInputStreamFromFile(InputStream * inStream, char * filename);
int seekInputStream(InputStream * inStream, long offset);
int finishInputStream(InputStream * inStream);

size_t fillBufferFromInputStream(InputStream * inStream, char * buffer, 
		size_t buflen);

#endif
