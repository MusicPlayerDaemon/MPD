/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "input/curl_input_plugin.h"
#include "input_plugin.h"
#include "conf.h"
#include "tag.h"
#include "icy_metadata.h"
#include "glib_compat.h"

#include <assert.h>

#if defined(WIN32)
	#include <winsock2.h>
#else
	#include <sys/select.h>
#endif

#include <string.h>
#include <errno.h>

#include <curl/curl.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_curl"

/**
 * Do not buffer more than this number of bytes.  It should be a
 * reasonable limit that doesn't make low-end machines suffer too
 * much, but doesn't cause stuttering on high-latency lines.
 */
static const size_t CURL_MAX_BUFFERED = 512 * 1024;

/**
 * Buffers created by input_curl_writefunction().
 */
struct buffer {
	/** size of the payload */
	size_t size;

	/** how much has been consumed yet? */
	size_t consumed;

	/** the payload */
	unsigned char data[sizeof(long)];
};

struct input_curl {
	struct input_stream base;

	/* some buffers which were passed to libcurl, which we have
	   too free */
	char *url, *range;
	struct curl_slist *request_headers;

	/** the curl handles */
	CURL *easy;
	CURLM *multi;

	/** list of buffers, where input_curl_writefunction() appends
	    to, and input_curl_read() reads from them */
	GQueue *buffers;

	/** has something been added to the buffers list? */
	bool buffered;

	/** did libcurl tell us the we're at the end of the response body? */
	bool eof;

	/** error message provided by libcurl */
	char error[CURL_ERROR_SIZE];

	/** parser for icy-metadata */
	struct icy_metadata icy_metadata;

	/** the stream name from the icy-name response header */
	char *meta_name;

	/** the tag object ready to be requested via
	    input_stream_tag() */
	struct tag *tag;
};

/** libcurl should accept "ICY 200 OK" */
static struct curl_slist *http_200_aliases;

/** HTTP proxy settings */
static const char *proxy, *proxy_user, *proxy_password;
static unsigned proxy_port;

static inline GQuark
curl_quark(void)
{
	return g_quark_from_static_string("curl");
}

static bool
input_curl_init(const struct config_param *param,
		G_GNUC_UNUSED GError **error_r)
{
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK) {
		g_warning("curl_global_init() failed: %s\n",
			  curl_easy_strerror(code));
		return false;
	}

	http_200_aliases = curl_slist_append(http_200_aliases, "ICY 200 OK");

	proxy = config_get_block_string(param, "proxy", NULL);
	proxy_port = config_get_block_unsigned(param, "proxy_port", 0);
	proxy_user = config_get_block_string(param, "proxy_user", NULL);
	proxy_password = config_get_block_string(param, "proxy_password",
						 NULL);

	if (proxy == NULL) {
		/* deprecated proxy configuration */
		proxy = config_get_string(CONF_HTTP_PROXY_HOST, NULL);
		proxy_port = config_get_positive(CONF_HTTP_PROXY_PORT, 0);
		proxy_user = config_get_string(CONF_HTTP_PROXY_USER, NULL);
		proxy_password = config_get_string(CONF_HTTP_PROXY_PASSWORD,
						   "");
	}

	return true;
}

static void
input_curl_finish(void)
{
	curl_slist_free_all(http_200_aliases);

	curl_global_cleanup();
}

/**
 * Determine the total sizes of all buffers, including portions that
 * have already been consumed.
 */
G_GNUC_PURE
static size_t
curl_total_buffer_size(const struct input_curl *c)
{
	size_t total = 0;

	for (GList *i = g_queue_peek_head_link(c->buffers);
	     i != NULL; i = g_list_next(i)) {
		struct buffer *buffer = i->data;
		total += buffer->size;
	}

	return total;
}

static void
buffer_free_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct buffer *buffer = data;

	assert(buffer->consumed <= buffer->size);

	g_free(data);
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

	g_queue_foreach(c->buffers, buffer_free_callback, NULL);
	g_queue_clear(c->buffers);
}

/**
 * Frees this stream (but not the input_stream struct itself).
 */
static void
input_curl_free(struct input_curl *c)
{
	if (c->tag != NULL)
		tag_free(c->tag);
	g_free(c->meta_name);

	input_curl_easy_free(c);

	if (c->multi != NULL)
		curl_multi_cleanup(c->multi);

	g_queue_free(c->buffers);

	g_free(c->url);
	input_stream_deinit(&c->base);
	g_free(c);
}

static struct tag *
input_curl_tag(struct input_stream *is)
{
	struct input_curl *c = (struct input_curl *)is;
	struct tag *tag = c->tag;

	c->tag = NULL;
	return tag;
}

static bool
input_curl_multi_info_read(struct input_curl *c, GError **error_r)
{
	CURLMsg *msg;
	int msgs_in_queue;

	while ((msg = curl_multi_info_read(c->multi,
					   &msgs_in_queue)) != NULL) {
		if (msg->msg == CURLMSG_DONE) {
			c->eof = true;
			c->base.ready = true;

			if (msg->data.result != CURLE_OK) {
				g_set_error(error_r, curl_quark(),
					    msg->data.result,
					    "curl failed: %s", c->error);
				return false;
			}
		}
	}

	return true;
}

/**
 * Wait for the libcurl socket.
 *
 * @return -1 on error, 0 if no data is available yet, 1 if data is
 * available
 */
static int
input_curl_select(struct input_curl *c, GError **error_r)
{
	fd_set rfds, wfds, efds;
	int max_fd, ret;
	CURLMcode mcode;
	struct timeval timeout = {
		.tv_sec = 1,
		.tv_usec = 0,
	};

	assert(!c->eof);

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

	mcode = curl_multi_fdset(c->multi, &rfds, &wfds, &efds, &max_fd);
	if (mcode != CURLM_OK) {
		g_set_error(error_r, curl_quark(), mcode,
			    "curl_multi_fdset() failed: %s",
			    curl_multi_strerror(mcode));
		return -1;
	}

#if LIBCURL_VERSION_NUM >= 0x070f04
	long timeout2;
	mcode = curl_multi_timeout(c->multi, &timeout2);
	if (mcode != CURLM_OK) {
		g_warning("curl_multi_timeout() failed: %s\n",
			  curl_multi_strerror(mcode));
		return -1;
	}

	if (timeout2 >= 0) {
		if (timeout2 > 10000)
			timeout2 = 10000;

		timeout.tv_sec = timeout2 / 1000;
		timeout.tv_usec = (timeout2 % 1000) * 1000;
	}
#endif

	ret = select(max_fd + 1, &rfds, &wfds, &efds, &timeout);
	if (ret < 0)
		g_set_error(error_r, g_quark_from_static_string("errno"),
			    errno,
			    "select() failed: %s\n", g_strerror(errno));

	return ret;
}

static bool
fill_buffer(struct input_stream *is, GError **error_r)
{
	struct input_curl *c = (struct input_curl *)is;
	CURLMcode mcode = CURLM_CALL_MULTI_PERFORM;

	while (!c->eof && g_queue_is_empty(c->buffers)) {
		int running_handles;
		bool bret;

		if (mcode != CURLM_CALL_MULTI_PERFORM) {
			/* if we're still here, there is no input yet
			   - wait for input */
			int ret = input_curl_select(c, error_r);
			if (ret <= 0)
				/* no data yet or error */
				return false;
		}

		mcode = curl_multi_perform(c->multi, &running_handles);
		if (mcode != CURLM_OK && mcode != CURLM_CALL_MULTI_PERFORM) {
			g_set_error(error_r, curl_quark(), mcode,
				    "curl_multi_perform() failed: %s",
				    curl_multi_strerror(mcode));
			c->eof = true;
			is->ready = true;
			return false;
		}

		bret = input_curl_multi_info_read(c, error_r);
		if (!bret)
			return false;
	}

	return !g_queue_is_empty(c->buffers);
}

/**
 * Mark a part of the buffer object as consumed.
 */
static struct buffer *
consume_buffer(struct buffer *buffer, size_t length)
{
	assert(buffer != NULL);
	assert(buffer->consumed < buffer->size);

	buffer->consumed += length;
	if (buffer->consumed < buffer->size)
		return buffer;

	assert(buffer->consumed == buffer->size);

	g_free(buffer);

	return NULL;
}

static size_t
read_from_buffer(struct icy_metadata *icy_metadata, GQueue *buffers,
		 void *dest0, size_t length)
{
	struct buffer *buffer = g_queue_pop_head(buffers);
	uint8_t *dest = dest0;
	size_t nbytes = 0;

	assert(buffer->size > 0);
	assert(buffer->consumed < buffer->size);

	if (length > buffer->size - buffer->consumed)
		length = buffer->size - buffer->consumed;

	while (true) {
		size_t chunk;

		chunk = icy_data(icy_metadata, length);
		if (chunk > 0) {
			memcpy(dest, buffer->data + buffer->consumed,
			       chunk);
			buffer = consume_buffer(buffer, chunk);

			nbytes += chunk;
			dest += chunk;
			length -= chunk;

			if (length == 0)
				break;

			assert(buffer != NULL);
		}

		chunk = icy_meta(icy_metadata, buffer->data + buffer->consumed,
				 length);
		if (chunk > 0) {
			buffer = consume_buffer(buffer, chunk);

			length -= chunk;

			if (length == 0)
				break;

			assert(buffer != NULL);
		}
	}

	if (buffer != NULL)
		g_queue_push_head(buffers, buffer);

	return nbytes;
}

static void
copy_icy_tag(struct input_curl *c)
{
	struct tag *tag = icy_tag(&c->icy_metadata);

	if (tag == NULL)
		return;

	if (c->tag != NULL)
		tag_free(c->tag);

	if (c->meta_name != NULL && !tag_has_type(tag, TAG_NAME))
		tag_add_item(tag, TAG_NAME, c->meta_name);

	c->tag = tag;
}

static size_t
input_curl_read(struct input_stream *is, void *ptr, size_t size,
		GError **error_r)
{
	struct input_curl *c = (struct input_curl *)is;
	bool success;
	size_t nbytes = 0;
	char *dest = ptr;

	do {
		/* fill the buffer */

		success = fill_buffer(is, error_r);
		if (!success)
			return 0;

		/* send buffer contents */

		while (size > 0 && !g_queue_is_empty(c->buffers)) {
			size_t copy = read_from_buffer(&c->icy_metadata, c->buffers,
						       dest + nbytes, size);

			nbytes += copy;
			size -= copy;
		}
	} while (nbytes == 0);

	if (icy_defined(&c->icy_metadata))
		copy_icy_tag(c);

	is->offset += (goffset)nbytes;

	return nbytes;
}

static void
input_curl_close(struct input_stream *is)
{
	struct input_curl *c = (struct input_curl *)is;

	input_curl_free(c);
}

static bool
input_curl_eof(G_GNUC_UNUSED struct input_stream *is)
{
	struct input_curl *c = (struct input_curl *)is;

	return c->eof && g_queue_is_empty(c->buffers);
}

static int
input_curl_buffer(struct input_stream *is, GError **error_r)
{
	struct input_curl *c = (struct input_curl *)is;

	if (curl_total_buffer_size(c) >= CURL_MAX_BUFFERED)
		return 0;

	CURLMcode mcode;
	int running_handles;
	bool ret;

	c->buffered = false;

	if (!is->ready && !c->eof)
		/* not ready yet means the caller is waiting in a busy
		   loop; relax that by calling select() on the
		   socket */
		if (input_curl_select(c, error_r) < 0)
			return -1;

	do {
		mcode = curl_multi_perform(c->multi, &running_handles);
	} while (mcode == CURLM_CALL_MULTI_PERFORM &&
		 g_queue_is_empty(c->buffers));

	if (mcode != CURLM_OK && mcode != CURLM_CALL_MULTI_PERFORM) {
		g_set_error(error_r, curl_quark(), mcode,
			    "curl_multi_perform() failed: %s",
			    curl_multi_strerror(mcode));
		c->eof = true;
		is->ready = true;
		return -1;
	}

	ret = input_curl_multi_info_read(c, error_r);
	if (!ret)
		return -1;

	return c->buffered;
}

/** called by curl when new data is available */
static size_t
input_curl_headerfunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct input_curl *c = (struct input_curl *)stream;
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

	if (g_ascii_strcasecmp(name, "accept-ranges") == 0) {
		/* a stream with icy-metadata is not seekable */
		if (!icy_defined(&c->icy_metadata))
			c->base.seekable = true;
	} else if (g_ascii_strcasecmp(name, "content-length") == 0) {
		char buffer[64];

		if ((size_t)(end - header) >= sizeof(buffer))
			return size;

		memcpy(buffer, value, end - value);
		buffer[end - value] = 0;

		c->base.size = c->base.offset + g_ascii_strtoull(buffer, NULL, 10);
	} else if (g_ascii_strcasecmp(name, "content-type") == 0) {
		g_free(c->base.mime);
		c->base.mime = g_strndup(value, end - value);
	} else if (g_ascii_strcasecmp(name, "icy-name") == 0 ||
		   g_ascii_strcasecmp(name, "ice-name") == 0 ||
		   g_ascii_strcasecmp(name, "x-audiocast-name") == 0) {
		g_free(c->meta_name);
		c->meta_name = g_strndup(value, end - value);

		if (c->tag != NULL)
			tag_free(c->tag);

		c->tag = tag_new();
		tag_add_item(c->tag, TAG_NAME, c->meta_name);
	} else if (g_ascii_strcasecmp(name, "icy-metaint") == 0) {
		char buffer[64];
		size_t icy_metaint;

		if ((size_t)(end - header) >= sizeof(buffer) ||
		    icy_defined(&c->icy_metadata))
			return size;

		memcpy(buffer, value, end - value);
		buffer[end - value] = 0;

		icy_metaint = g_ascii_strtoull(buffer, NULL, 10);
		g_debug("icy-metaint=%zu", icy_metaint);

		if (icy_metaint > 0) {
			icy_start(&c->icy_metadata, icy_metaint);

			/* a stream with icy-metadata is not
			   seekable */
			c->base.seekable = false;
		}
	}

	return size;
}

/** called by curl when new data is available */
static size_t
input_curl_writefunction(void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct input_curl *c = (struct input_curl *)stream;
	struct buffer *buffer;

	size *= nmemb;
	if (size == 0)
		return 0;

	buffer = g_malloc(sizeof(*buffer) - sizeof(buffer->data) + size);
	buffer->size = size;
	buffer->consumed = 0;
	memcpy(buffer->data, ptr, size);
	g_queue_push_tail(c->buffers, buffer);

	c->buffered = true;
	c->base.ready = true;

	return size;
}

static bool
input_curl_easy_init(struct input_curl *c, GError **error_r)
{
	CURLcode code;
	CURLMcode mcode;

	c->eof = false;

	c->easy = curl_easy_init();
	if (c->easy == NULL) {
		g_set_error(error_r, curl_quark(), 0,
			    "curl_easy_init() failed");
		return false;
	}

	mcode = curl_multi_add_handle(c->multi, c->easy);
	if (mcode != CURLM_OK) {
		g_set_error(error_r, curl_quark(), mcode,
			    "curl_multi_add_handle() failed: %s",
			    curl_multi_strerror(mcode));
		return false;
	}

	curl_easy_setopt(c->easy, CURLOPT_USERAGENT,
			 "Music Player Daemon " VERSION);
	curl_easy_setopt(c->easy, CURLOPT_HEADERFUNCTION,
			 input_curl_headerfunction);
	curl_easy_setopt(c->easy, CURLOPT_WRITEHEADER, c);
	curl_easy_setopt(c->easy, CURLOPT_WRITEFUNCTION,
			 input_curl_writefunction);
	curl_easy_setopt(c->easy, CURLOPT_WRITEDATA, c);
	curl_easy_setopt(c->easy, CURLOPT_HTTP200ALIASES, http_200_aliases);
	curl_easy_setopt(c->easy, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(c->easy, CURLOPT_NETRC, 1);
	curl_easy_setopt(c->easy, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(c->easy, CURLOPT_FAILONERROR, true);
	curl_easy_setopt(c->easy, CURLOPT_ERRORBUFFER, c->error);

	if (proxy != NULL)
		curl_easy_setopt(c->easy, CURLOPT_PROXY, proxy);

	if (proxy_port > 0)
		curl_easy_setopt(c->easy, CURLOPT_PROXYPORT, (long)proxy_port);

	if (proxy_user != NULL && proxy_password != NULL) {
		char *proxy_auth_str =
			g_strconcat(proxy_user, ":", proxy_password, NULL);
		curl_easy_setopt(c->easy, CURLOPT_PROXYUSERPWD, proxy_auth_str);
		g_free(proxy_auth_str);
	}

	code = curl_easy_setopt(c->easy, CURLOPT_URL, c->url);
	if (code != CURLE_OK) {
		g_set_error(error_r, curl_quark(), code,
			    "curl_easy_setopt() failed: %s",
			    curl_easy_strerror(code));
		return false;
	}

	c->request_headers = NULL;
	c->request_headers = curl_slist_append(c->request_headers,
					       "Icy-Metadata: 1");
	curl_easy_setopt(c->easy, CURLOPT_HTTPHEADER, c->request_headers);

	return true;
}

static bool
input_curl_send_request(struct input_curl *c, GError **error_r)
{
	CURLMcode mcode;
	int running_handles;

	do {
		mcode = curl_multi_perform(c->multi, &running_handles);
	} while (mcode == CURLM_CALL_MULTI_PERFORM);

	if (mcode != CURLM_OK) {
		g_set_error(error_r, curl_quark(), mcode,
			    "curl_multi_perform() failed: %s",
			    curl_multi_strerror(mcode));
		return false;
	}

	return true;
}

static bool
input_curl_seek(struct input_stream *is, goffset offset, int whence,
		GError **error_r)
{
	struct input_curl *c = (struct input_curl *)is;
	bool ret;

	assert(is->ready);

	if (whence == SEEK_SET && offset == is->offset)
		/* no-op */
		return true;

	if (!is->seekable)
		return false;

	/* calculate the absolute offset */

	switch (whence) {
	case SEEK_SET:
		break;

	case SEEK_CUR:
		offset += is->offset;
		break;

	case SEEK_END:
		if (is->size < 0)
			/* stream size is not known */
			return false;

		offset += is->size;
		break;

	default:
		return false;
	}

	if (offset < 0)
		return false;

	/* check if we can fast-forward the buffer */

	while (offset > is->offset && !g_queue_is_empty(c->buffers)) {
		struct buffer *buffer;
		size_t length;

		buffer = (struct buffer *)g_queue_pop_head(c->buffers);

		length = buffer->size - buffer->consumed;
		if (offset - is->offset < (goffset)length)
			length = offset - is->offset;

		buffer = consume_buffer(buffer, length);
		if (buffer != NULL)
			g_queue_push_head(c->buffers, buffer);

		is->offset += length;
	}

	if (offset == is->offset)
		return true;

	/* close the old connection and open a new one */

	input_curl_easy_free(c);

	is->offset = offset;
	if (is->offset == is->size) {
		/* seek to EOF: simulate empty result; avoid
		   triggering a "416 Requested Range Not Satisfiable"
		   response */
		c->eof = true;
		return true;
	}

	ret = input_curl_easy_init(c, error_r);
	if (!ret)
		return false;

	/* send the "Range" header */

	if (is->offset > 0) {
		c->range = g_strdup_printf("%lld-", (long long)is->offset);
		curl_easy_setopt(c->easy, CURLOPT_RANGE, c->range);
	}

	ret = input_curl_send_request(c, error_r);
	if (!ret)
		return false;

	return input_curl_multi_info_read(c, error_r);
}

static struct input_stream *
input_curl_open(const char *url, GError **error_r)
{
	struct input_curl *c;
	bool ret;

	if (strncmp(url, "http://", 7) != 0)
		return NULL;

	c = g_new0(struct input_curl, 1);
	input_stream_init(&c->base, &input_plugin_curl, url);

	c->url = g_strdup(url);
	c->buffers = g_queue_new();

	c->multi = curl_multi_init();
	if (c->multi == NULL) {
		g_set_error(error_r, curl_quark(), 0,
			    "curl_multi_init() failed");
		input_curl_free(c);
		return NULL;
	}

	icy_clear(&c->icy_metadata);
	c->tag = NULL;

	ret = input_curl_easy_init(c, error_r);
	if (!ret) {
		input_curl_free(c);
		return NULL;
	}

	ret = input_curl_send_request(c, error_r);
	if (!ret) {
		input_curl_free(c);
		return NULL;
	}

	ret = input_curl_multi_info_read(c, error_r);
	if (!ret) {
		input_curl_free(c);
		return NULL;
	}

	return &c->base;
}

const struct input_plugin input_plugin_curl = {
	.name = "curl",
	.init = input_curl_init,
	.finish = input_curl_finish,

	.open = input_curl_open,
	.close = input_curl_close,
	.tag = input_curl_tag,
	.buffer = input_curl_buffer,
	.read = input_curl_read,
	.eof = input_curl_eof,
	.seek = input_curl_seek,
};
