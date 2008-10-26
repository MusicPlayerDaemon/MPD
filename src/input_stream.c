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

static const struct input_plugin *const input_plugins[] = {
	&input_plugin_file,
#ifdef HAVE_CURL
	&input_plugin_curl,
#endif
};

static const unsigned num_input_plugins =
	sizeof(input_plugins) / sizeof(input_plugins[0]);

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

bool
input_stream_open(struct input_stream *is, char *url)
{
	is->seekable = false;
	is->ready = false;
	is->offset = 0;
	is->size = 0;
	is->error = 0;
	is->mime = NULL;
	is->meta_name = NULL;
	is->meta_title = NULL;

	for (unsigned i = 0; i < num_input_plugins; ++i) {
		const struct input_plugin *plugin = input_plugins[i];

		if (plugin->open(is, url)) {
			is->plugin = plugin;
			return true;
		}
	}

	return false;
}

bool
input_stream_seek(struct input_stream *is, long offset, int whence)
{
	return is->plugin->seek(is, offset, whence);
}

size_t
input_stream_read(struct input_stream *is, void *ptr, size_t size)
{
	return is->plugin->read(is, ptr, size);
}

void input_stream_close(struct input_stream *is)
{
	if (is->mime)
		free(is->mime);
	if (is->meta_name)
		free(is->meta_name);
	if (is->meta_title)
		free(is->meta_title);

	is->plugin->close(is);
}

bool input_stream_eof(struct input_stream *is)
{
	return is->plugin->eof(is);
}

int input_stream_buffer(struct input_stream *is)
{
	return is->plugin->buffer(is);
}
