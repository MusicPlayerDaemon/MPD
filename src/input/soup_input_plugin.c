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
#include "input/soup_input_plugin.h"
#include "input_internal.h"
#include "input_plugin.h"
#include "io_thread.h"
#include "conf.h"

#include <libsoup/soup-uri.h>
#include <libsoup/soup-session-async.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_soup"

/**
 * Do not buffer more than this number of bytes.  It should be a
 * reasonable limit that doesn't make low-end machines suffer too
 * much, but doesn't cause stuttering on high-latency lines.
 */
static const size_t SOUP_MAX_BUFFERED = 512 * 1024;

static SoupURI *soup_proxy;
static SoupSession *soup_session;

struct input_soup {
	struct input_stream base;

	GMutex *mutex;
	GCond *cond;

	SoupMessage *msg;

	GQueue *buffers;

	size_t current_consumed;

	size_t total_buffered;

	bool alive, pause, eof;
};

static inline GQuark
soup_quark(void)
{
	return g_quark_from_static_string("soup");
}

static bool
input_soup_init(const struct config_param *param, GError **error_r)
{
	assert(soup_proxy == NULL);
	assert(soup_session == NULL);

	g_type_init();

	const char *proxy = config_get_block_string(param, "proxy", NULL);

	if (proxy != NULL) {
		soup_proxy = soup_uri_new(proxy);
		if (soup_proxy == NULL) {
			g_set_error(error_r, soup_quark(), 0,
				    "failed to parse proxy setting");
			return false;
		}
	}

	soup_session =
		soup_session_async_new_with_options(SOUP_SESSION_PROXY_URI,
						    soup_proxy,
						    SOUP_SESSION_ASYNC_CONTEXT,
						    io_thread_context(),
						    NULL);

	return true;
}

static void
input_soup_finish(void)
{
	assert(soup_session != NULL);

	soup_session_abort(soup_session);
	g_object_unref(G_OBJECT(soup_session));

	if (soup_proxy != NULL)
		soup_uri_free(soup_proxy);
}

static void
input_soup_session_callback(G_GNUC_UNUSED SoupSession *session,
			    G_GNUC_UNUSED SoupMessage *msg, gpointer user_data)
{
	struct input_soup *s = user_data;

	assert(msg == s->msg);

	g_mutex_lock(s->mutex);
	s->base.ready = true;
	s->alive = false;
	g_cond_broadcast(s->cond);
	g_mutex_unlock(s->mutex);
}

static void
input_soup_got_headers(SoupMessage *msg, gpointer user_data)
{
	struct input_soup *s = user_data;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		soup_session_cancel_message(soup_session, msg,
					    SOUP_STATUS_CANCELLED);
		return;
	}

	soup_message_body_set_accumulate(msg->response_body, false);

	g_mutex_lock(s->mutex);
	s->base.ready = true;
	g_cond_broadcast(s->cond);
	g_mutex_unlock(s->mutex);
}

static void
input_soup_got_chunk(SoupMessage *msg, SoupBuffer *chunk, gpointer user_data)
{
	struct input_soup *s = user_data;

	assert(msg == s->msg);

	g_mutex_lock(s->mutex);

	g_queue_push_tail(s->buffers, soup_buffer_copy(chunk));
	s->total_buffered += chunk->length;

	if (s->total_buffered >= SOUP_MAX_BUFFERED && !s->pause) {
		s->pause = true;
		soup_session_pause_message(soup_session, msg);
	}

	g_cond_broadcast(s->cond);
	g_mutex_unlock(s->mutex);
}

static void
input_soup_got_body(G_GNUC_UNUSED SoupMessage *msg, gpointer user_data)
{
	struct input_soup *s = user_data;

	assert(msg == s->msg);

	g_mutex_lock(s->mutex);

	s->base.ready = true;
	s->eof = true;
	s->alive = false;

	g_cond_broadcast(s->cond);
	g_mutex_unlock(s->mutex);
}

static bool
input_soup_wait_data(struct input_soup *s)
{
	while (true) {
		if (s->eof)
			return true;

		if (!s->alive)
			return false;

		if (!g_queue_is_empty(s->buffers))
			return true;

		assert(s->current_consumed == 0);

		g_cond_wait(s->cond, s->mutex);
	}
}

static struct input_stream *
input_soup_open(const char *uri, G_GNUC_UNUSED GError **error_r)
{
	if (strncmp(uri, "http://", 7) != 0)
		return NULL;

	struct input_soup *s = g_new(struct input_soup, 1);
	input_stream_init(&s->base, &input_plugin_soup, uri);

	s->mutex = g_mutex_new();
	s->cond = g_cond_new();

	s->buffers = g_queue_new();
	s->current_consumed = 0;
	s->total_buffered = 0;

	s->msg = soup_message_new(SOUP_METHOD_GET, uri);
	soup_message_set_flags(s->msg, SOUP_MESSAGE_NO_REDIRECT);

	soup_message_headers_append(s->msg->request_headers, "User-Agent",
				    "Music Player Daemon " VERSION);

	g_signal_connect(s->msg, "got-headers",
			 G_CALLBACK(input_soup_got_headers), s);
	g_signal_connect(s->msg, "got-chunk",
			 G_CALLBACK(input_soup_got_chunk), s);
	g_signal_connect(s->msg, "got-body",
			 G_CALLBACK(input_soup_got_body), s);

	s->alive = true;
	s->pause = false;
	s->eof = false;

	soup_session_queue_message(soup_session, s->msg,
				   input_soup_session_callback, s);

	return &s->base;
}

static void
input_soup_close(struct input_stream *is)
{
	struct input_soup *s = (struct input_soup *)is;

	g_mutex_lock(s->mutex);

	if (s->alive) {
		assert(s->msg != NULL);

		s->alive = false;
		g_mutex_unlock(s->mutex);

		soup_session_cancel_message(soup_session, s->msg,
					    SOUP_STATUS_CANCELLED);
	} else
		g_mutex_unlock(s->mutex);

	g_mutex_free(s->mutex);
	g_cond_free(s->cond);

	SoupBuffer *buffer;
	while ((buffer = g_queue_pop_head(s->buffers)) != NULL)
		soup_buffer_free(buffer);
	g_queue_free(s->buffers);

	input_stream_deinit(&s->base);
	g_free(s);
}

static int
input_soup_buffer(struct input_stream *is, GError **error_r)
{
	struct input_soup *s = (struct input_soup *)is;

	g_mutex_lock(s->mutex);

	if (s->pause) {
		if (s->total_buffered >= SOUP_MAX_BUFFERED) {
			g_mutex_unlock(s->mutex);
			return 1;
		}

		s->pause = false;
		soup_session_unpause_message(soup_session, s->msg);
	}


	bool success = input_soup_wait_data(s);
	g_mutex_unlock(s->mutex);

	if (!success) {
		g_set_error_literal(error_r, soup_quark(), 0, "HTTP failure");
		return -1;
	}

	return 1;
}

static size_t
input_soup_read(struct input_stream *is, void *ptr, size_t size,
		G_GNUC_UNUSED GError **error_r)
{
	struct input_soup *s = (struct input_soup *)is;

	g_mutex_lock(s->mutex);

	if (!input_soup_wait_data(s)) {
		assert(!s->alive);
		g_mutex_unlock(s->mutex);

		g_set_error_literal(error_r, soup_quark(), 0, "HTTP failure");
		return 0;
	}

	char *p0 = ptr, *p = p0, *p_end = p0 + size;

	while (p < p_end) {
		SoupBuffer *buffer = g_queue_pop_head(s->buffers);
		if (buffer == NULL) {
			assert(s->current_consumed == 0);
			break;
		}

		assert(s->current_consumed < buffer->length);
		assert(s->total_buffered >= buffer->length);

		const char *q = buffer->data;
		q += s->current_consumed;

		size_t remaining = buffer->length - s->current_consumed;
		size_t nbytes = p_end - p;
		if (nbytes > remaining)
			nbytes = remaining;

		memcpy(p, q, nbytes);
		p += nbytes;

		s->current_consumed += remaining;
		if (s->current_consumed >= buffer->length) {
			/* done with this buffer */
			s->total_buffered -= buffer->length;
			soup_buffer_free(buffer);
			s->current_consumed = 0;
		} else {
			/* partial read */
			assert(p == p_end);

			g_queue_push_head(s->buffers, buffer);
		}
	}

	if (s->pause && s->total_buffered < SOUP_MAX_BUFFERED) {
		s->pause = false;
		soup_session_unpause_message(soup_session, s->msg);
	}

	size_t nbytes = p - p0;
	s->base.offset += nbytes;

	g_mutex_unlock(s->mutex);
	return nbytes;
}

static bool
input_soup_eof(G_GNUC_UNUSED struct input_stream *is)
{
	struct input_soup *s = (struct input_soup *)is;

	return !s->alive && g_queue_is_empty(s->buffers);
}

const struct input_plugin input_plugin_soup = {
	.name = "soup",
	.init = input_soup_init,
	.finish = input_soup_finish,

	.open = input_soup_open,
	.close = input_soup_close,
	.buffer = input_soup_buffer,
	.read = input_soup_read,
	.eof = input_soup_eof,
};
