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

#include "input_stream.h"
#include "config.h"

#include "input_file.h"

#ifdef HAVE_CURL
#include "input_curl.h"
#endif

#include <stdlib.h>

void input_stream_global_init(void)
{
#ifdef HAVE_CURL
	input_curl_global_init();
#endif
}

void input_stream_global_finish(void)
{
#ifdef HAVE_CURL
	input_curl_global_finish();
#endif
}

int input_stream_open(struct input_stream *is, char *url)
{
	is->ready = 0;
	is->offset = 0;
	is->size = 0;
	is->error = 0;
	is->mime = NULL;
	is->seekable = 0;
	is->meta_name = NULL;
	is->meta_title = NULL;

	if (input_file_open(is, url) == 0)
		return 0;

#ifdef HAVE_CURL
	if (input_curl_open(is, url))
		return 0;
#endif

	return -1;
}

int input_stream_seek(struct input_stream *is, long offset, int whence)
{
	return is->seekFunc(is, offset, whence);
}

size_t
input_stream_read(struct input_stream *is, void *ptr, size_t size)
{
	return is->readFunc(is, ptr, size);
}

int input_stream_close(struct input_stream *is)
{
	if (is->mime)
		free(is->mime);
	if (is->meta_name)
		free(is->meta_name);
	if (is->meta_title)
		free(is->meta_title);

	return is->closeFunc(is);
}

int input_stream_eof(struct input_stream *is)
{
	return is->atEOFFunc(is);
}

int input_stream_buffer(struct input_stream *is)
{
	return is->bufferFunc(is);
}
