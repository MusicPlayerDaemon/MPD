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

#ifndef INPUT_STREAM_H
#define INPUT_STREAM_H

#include <stddef.h>

typedef struct input_stream InputStream;

struct input_stream {
	int ready;

	int error;
	long offset;
	size_t size;
	char *mime;
	int seekable;

	int (*seekFunc)(struct input_stream *inStream, long offset,
			int whence);
	size_t (*readFunc)(struct input_stream *inStream, void *ptr,
			   size_t size);
	int (*closeFunc)(struct input_stream *inStream);
	int (*atEOFFunc)(struct input_stream *inStream);
	int (*bufferFunc)(struct input_stream *inStream);

	void *data;
	char *metaName;
	char *metaTitle;
};

void initInputStream(void);

int isUrlSaneForInputStream(char *url);

/* if an error occurs for these 3 functions, then -1 is returned and errno
   for the input stream is set */
int openInputStream(struct input_stream *inStream, char *url);
int seekInputStream(struct input_stream *inStream, long offset, int whence);
int closeInputStream(struct input_stream *inStream);
int inputStreamAtEOF(struct input_stream *inStream);

/* return value: -1 is error, 1 inidicates stuff was buffered, 0 means nothing
   was buffered */
int bufferInputStream(struct input_stream *inStream);

size_t readFromInputStream(struct input_stream *inStream,
			   void *ptr, size_t size);

#endif
