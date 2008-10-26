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
#include <stdbool.h>

struct input_stream;

struct input_plugin {
	bool (*open)(struct input_stream *is, const char *url);
	void (*close)(struct input_stream *is);

	int (*buffer)(struct input_stream *is);
	size_t (*read)(struct input_stream *is, void *ptr, size_t size);
	int (*eof)(struct input_stream *is);
	int (*seek)(struct input_stream *is, long offset, int whence);
};

struct input_stream {
	const struct input_plugin *plugin;

	int ready;

	int error;
	long offset;
	size_t size;
	char *mime;
	int seekable;

	void *data;
	char *meta_name;
	char *meta_title;
};

void input_stream_global_init(void);

void input_stream_global_finish(void);

/* if an error occurs for these 3 functions, then -1 is returned and errno
   for the input stream is set */
int input_stream_open(struct input_stream *is, char *url);
int input_stream_seek(struct input_stream *is, long offset, int whence);
void input_stream_close(struct input_stream *is);
int input_stream_eof(struct input_stream *is);

/* return value: -1 is error, 1 inidicates stuff was buffered, 0 means nothing
   was buffered */
int input_stream_buffer(struct input_stream *is);

size_t
input_stream_read(struct input_stream *is, void *ptr, size_t size);

#endif
