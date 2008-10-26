/* the Music Player Daemon (MPD)
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

#include "input_curl.h"
#include "input_stream.h"
#include "gcc.h"
#include "dlist.h"

#include <assert.h>
#include <sys/select.h>
#include <string.h>
#include <errno.h>

#include <curl/curl.h>
#include <glib.h>

/**
 * Buffers created by input_curl_writefunction().
 */
struct buffer {
	struct list_head siblings;

	/** size of the payload */
	size_t size;

	/** how much has been consumed yet? */
	size_t consumed;

	/** the payload */
	unsigned char data[sizeof(long)];
};

struct input_curl {
	/* some buffers which were passed to libcurl, which we have
	   too free */
	char *url, *range;
	struct curl_slist *request_headers;

	/** the curl handles */
	CURL *easy;
	CURLM *multi;

	/** list of buffers, where input_curl_writefunction() appends
	    to, and input_curl_read() reads from them */
	struct list_head buffers;

	/** has something been added to the buffers list? */
	bool buffered;

	/** did libcurl tell us the we're at the end of the response body? */
	bool eof;
};

/** libcurl should accept "ICY 200 OK" */
static struct curl_slist *http_200_aliases;

void input_curl_global_init(void)
{
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK)
		g_warning("curl_global_init() failed: %s\n",
			  curl_easy_strerror(code));

	http_200_aliases = curl_slist_append(http_200_aliases, "ICY 200 OK");
}

void input_curl_global_finish(void)
{
	curl_slist_free_all(http_200_aliases);

	curl_global_cleanup();
}

/**
 * Frees the current "libcurl easy" handle, and everything associated
 * with it.
 */
static void
input_curl_easy_free(struct input_curl *c)
{
	if (c->easy != NULL) {
		curl_multi_remove_handle(c->multi, c->easy);
		curl_easy_cleanup(c->easy);
		c->easy = NULL;
	}

	curl_slist_free_all(c->request_headers);
	c->request_headers = NULL;

	g_free(c->range);
	c->range = NULL;

	while (!list_empty(&c->buffers)) {
		struct buffer *buffer = (struct buffer *)c->buffers.next;
		list_del(&buffer->siblings);

		g_free(buffer);
	}
}

/**
 * Frees this stream (but not the input_stream struct itself).
 */
static void
input_curl_free(struct input_stream *is)
{
	struct input_curl *c = is->data;

	input_curl_easy_free(c);

	if (c->multi != NULL)
		curl_multi_cleanup(c->multi);

	g_free(c->url);
	g_free(c);
}

/**
 * Wait for the libcurl socket.
 *
 * @return -1 on error, 0 if no data is available yet, 1 if data is
 * available
 */
static int
input_curl_select(struct input_curl *c)
{
	fd_set rfds, wfds, efds;
	int max_fd, ret;
	CURLMcode mcode;
	/* XXX hard coded timeout value.. */
	struct timeval timeout = {
		.tv_sec = 1,
		.tv_usec = 0,
	};

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

	mcode = curl_multi_fdset(c->multi, &rfds, &wfds, &efds, &max_fd);
	if (mcode != CURLM_OK) {
		g_warning("curl_multi_fdset() failed: %s\n",
			  curl_multi_strerror(mcode));
		return -1;
	}

	assert(max_fd >= 0);

	ret = select(max_fd + 1, &rfds, &wfds, &efds, &timeout);
	if (ret < 0)
		g_warning("select() failed: %s\n", strerror(errno));

	return ret;
}

static size_t
read_from_buffer(struct buffer *buffer, void *dest, size_t length)
{
	assert(buffer->size > 0);
	assert(buffer->consumed < buffer->size);

	if (length > buffer->size - buffer->consumed)
		length = buffer->size - buffer->consumed;

	memcpy(dest, buffer->data + buffer->consumed, length);

	buffer->consumed += length;
	if (buffer->consumed == buffer->size) {
		list_del(&buffer->siblings);
		g_free(buffer);
	}

	return length;
}

static size_t
input_curl_read(struct input_stream *is, void *ptr, size_t size)
{
	struct input_curl *c = is->data;
	CURLMcode mcode = CURLM_CALL_MULTI_PERFORM;
	size_t nbytes = 0;
	char *dest = ptr;

	/* fill the buffer */

	while (!c->eof && list_empty(&c->buffers)) {
		int running_handles;

		if (mcode != CURLM_CALL_MULTI_PERFORM) {
			/* if we're still here, there is no input yet
			   - wait for input */
			int ret = input_curl_select(c);
			if (ret <= 0)
				/* no data yet or error */
				return 0;
		}

		mcode = curl_multi_perform(c->multi, &running_handles);
		if (mcode != CURLM_OK && mcode != CURLM_CALL_MULTI_PERFORM) {
			g_warning("curl_multi_perform() failed: %s\n",
				  curl_multi_strerror(mcode));
			c->eof = true;
			return 0;
		}

		c->eof = running_handles == 0;
	}

	/* send buffer contents */

	while (size > 0 && !list_empty(&c->buffers)) {
		struct buffer *buffer = (struct buffer *)c->buffers.next;
		size_t copy = read_from_buffer(buffer, dest + nbytes, size);

		nbytes += copy;
		size -= copy;
	}

	is->offset += (off_t)nbytes;
	return nbytes;
}

static void
input_curl_close(struct input_stream *is)
{
	input_curl_free(is);
}

static bool
input_curl_eof(mpd_unused struct input_stream *is)
{
	struct input_curl *c = is->data;

	return c->eof && list_empty(&c->buffers);
}

static int
input_curl_buffer(mpd_unused struct input_stream *is)
{
	struct input_curl *c = is->data;
	CURLMcode mcode;
	int running_handles;

	c->buffered = false;

	do {
		mcode = curl_multi_perform(c->multi, &running_handles);
	} while (mcode == CURLM_CALL_MULTI_PERFORM && list_empty(&c->buffers));

	if (mcode != CURLM_OK && mcode != CURLM_CALL_MULTI_PERFORM) {
		g_warning("curl_multi_perform() failed: %s\n",
			  curl_multi_strerror(mcode));
		c->eof = true;
		return -1;
	}

	c->eof = running_handles == 0;

	return c->buffered;
}

/** called by curl when new data is available */
static size_t
input_curl_headerfunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct input_stream *is = stream;
	const char *header = ptr, *end, *value;
	char name[64];

	size *= nmemb;
	end = header + size;

	value = memchr(header, ':', size);
	if (value == NULL || (size_t)(value - header) >= sizeof(name))
		return size;

	memcpy(name, header, value - header);
	name[value - header] = 0;

	/* skip the colon */

	++value;

	/* strip the value */

	while (value < end && g_ascii_isspace(*value))
		++value;

	while (end > value && g_ascii_isspace(end[-1]))
		--end;

	if (strcasecmp(name, "accept-ranges") == 0)
		is->seekable = true;
	else if (strcasecmp(name, "content-length") == 0) {
		char buffer[64];

		if ((size_t)(end - header) >= sizeof(buffer))
			return size;

		memcpy(buffer, value, end - value);
		buffer[end - value] = 0;

		is->size = is->offset + g_ascii_strtoull(buffer, NULL, 10);
	} else if (strcasecmp(name, "content-type") == 0) {
		g_free(is->mime);
		is->mime = g_strndup(value, end - value);
	} else if (strcasecmp(name, "icy-name") == 0 ||
		   strcasecmp(name, "ice-name") == 0 ||
		   strcasecmp(name, "x-audiocast-name") == 0) {
		g_free(is->meta_name);
		is->meta_name = g_strndup(value, end - value);
	}

	return size;
}

/** called by curl when new data is available */
static size_t
input_curl_writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct input_stream *is = stream;
	struct input_curl *c = is->data;
	struct buffer *buffer;

	size *= nmemb;
	if (size == 0)
		return 0;

	buffer = g_malloc(sizeof(*buffer) - sizeof(buffer->data) + size);
	buffer->size = size;
	buffer->consumed = 0;
	memcpy(buffer->data, ptr, size);
	list_add_tail(&buffer->siblings, &c->buffers);

	c->buffered = true;
	is->ready = true;

	return size;
}

static bool
input_curl_easy_init(struct input_stream *is)
{
	struct input_curl *c = is->data;
	CURLcode code;
	CURLMcode mcode;

	c->eof = false;

	c->easy = curl_easy_init();
	if (c->easy == NULL) {
		g_warning("curl_easy_init() failed\n");
		return false;
	}

	mcode = curl_multi_add_handle(c->multi, c->easy);
	if (mcode != CURLM_OK)
		return false;

	curl_easy_setopt(c->easy, CURLOPT_HEADERFUNCTION,
			 input_curl_headerfunction);
	curl_easy_setopt(c->easy, CURLOPT_WRITEHEADER, is);
	curl_easy_setopt(c->easy, CURLOPT_WRITEFUNCTION,
			 input_curl_writefunction);
	curl_easy_setopt(c->easy, CURLOPT_WRITEDATA, is);
	curl_easy_setopt(c->easy, CURLOPT_HTTP200ALIASES, http_200_aliases);

	code = curl_easy_setopt(c->easy, CURLOPT_URL, c->url);
	if (code != CURLE_OK)
		return false;

	c->request_headers = NULL;
	c->request_headers = curl_slist_append(c->request_headers,
					       "Icy-Metadata: 1");
	curl_easy_setopt(c->easy, CURLOPT_HTTPHEADER, c->request_headers);

	return true;
}

static bool
input_curl_send_request(struct input_curl *c)
{
	CURLMcode mcode;
	int running_handles;

	do {
		mcode = curl_multi_perform(c->multi, &running_handles);
	} while (mcode == CURLM_CALL_MULTI_PERFORM);

	c->eof = running_handles == 0;

	if (mcode != CURLM_OK) {
		g_warning("curl_multi_perform() failed: %s\n",
			  curl_multi_strerror(mcode));
		return false;
	}

	return true;
}

static bool
input_curl_seek(struct input_stream *is, mpd_unused long offset,
		mpd_unused int whence)
{
	struct input_curl *c = is->data;
	bool ret;

	if (!is->seekable)
		return false;

	/* calculate the absolute offset */

	switch (whence) {
	case SEEK_SET:
		is->offset = (off_t)offset;
		break;

	case SEEK_CUR:
		is->offset += (off_t)offset;
		break;

	case SEEK_END:
		is->offset = (off_t)is->size + (off_t)offset;
		break;

	default:
		return false;
	}

	if (is->offset < 0)
		return false;

	/* close the old connection and open a new one */

	input_curl_easy_free(c);

	ret = input_curl_easy_init(is);
	if (!ret)
		return false;

	/* send the "Range" header */

	if (is->offset > 0) {
		c->range = g_strdup_printf("%ld-", is->offset); /* XXX 64 bit safety */
		curl_easy_setopt(c->easy, CURLOPT_RANGE, c->range);
	}

	ret = input_curl_send_request(c);
	if (!ret)
		return false;

	return true;
}

static bool
input_curl_open(struct input_stream *is, const char *url)
{
	struct input_curl *c;
	bool ret;

	c = g_new0(struct input_curl, 1);
	c->url = g_strdup(url);
	INIT_LIST_HEAD(&c->buffers);

	is->data = c;

	c->multi = curl_multi_init();
	if (c->multi == NULL) {
		g_warning("curl_multi_init() failed\n");

		input_curl_free(is);
		return false;
	}

	ret = input_curl_easy_init(is);
	if (!ret) {
		input_curl_free(is);
		return false;
	}

	ret = input_curl_send_request(c);
	if (!ret) {
		input_curl_free(is);
		return false;
	}

	return true;
}

const struct input_plugin input_plugin_curl = {
	.open = input_curl_open,
	.close = input_curl_close,
	.buffer = input_curl_buffer,
	.read = input_curl_read,
	.eof = input_curl_eof,
	.seek = input_curl_seek,
};
