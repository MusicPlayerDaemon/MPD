/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "SoupInputPlugin.hxx"
#include "InputPlugin.hxx"
#include "InputStream.hxx"
#include "InputInternal.hxx"
#include "IOThread.hxx"
#include "event/Loop.hxx"
#include "conf.h"

extern "C" {
#include <libsoup/soup-uri.h>
#include <libsoup/soup-session-async.h>
}

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

/**
 * Resume the stream at this number of bytes after it has been paused.
 */
static const size_t SOUP_RESUME_AT = 384 * 1024;

static SoupURI *soup_proxy;
static SoupSession *soup_session;

struct SoupInputStream {
	struct input_stream base;

	SoupMessage *msg;

	GQueue *buffers;

	size_t current_consumed;

	size_t total_buffered;

	bool alive, pause, eof;

	/**
	 * Set when the session callback has been invoked, when it is
	 * safe to free this object.
	 */
	bool completed;

	GError *postponed_error;
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
						    io_thread_get().GetContext(),
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

/**
 * Copy the error from the SoupMessage object to
 * input_soup::postponed_error.
 *
 * @return true if there was no error
 */
static bool
input_soup_copy_error(SoupInputStream *s, const SoupMessage *msg)
{
	if (SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
		return true;

	if (msg->status_code == SOUP_STATUS_CANCELLED)
		/* failure, but don't generate a GError, because this
		   status was caused by _close() */
		return false;

	if (s->postponed_error != NULL)
		/* there's already a GError, don't overwrite it */
		return false;

	if (SOUP_STATUS_IS_TRANSPORT_ERROR(msg->status_code))
		s->postponed_error =
			g_error_new(soup_quark(), msg->status_code,
				    "HTTP client error: %s",
				    msg->reason_phrase);
	else
		s->postponed_error =
			g_error_new(soup_quark(), msg->status_code,
				    "got HTTP status: %d %s",
				    msg->status_code, msg->reason_phrase);

	return false;
}

static void
input_soup_session_callback(G_GNUC_UNUSED SoupSession *session,
			    SoupMessage *msg, gpointer user_data)
{
	SoupInputStream *s = (SoupInputStream *)user_data;

	assert(msg == s->msg);
	assert(!s->completed);

	const ScopeLock protect(*s->base.mutex);

	if (!s->base.ready)
		input_soup_copy_error(s, msg);

	s->base.ready = true;
	s->alive = false;
	s->completed = true;

	s->base.cond->broadcast();
}

static void
input_soup_got_headers(SoupMessage *msg, gpointer user_data)
{
	SoupInputStream *s = (SoupInputStream *)user_data;

	s->base.mutex->lock();

	if (!input_soup_copy_error(s, msg)) {
		s->base.mutex->unlock();

		soup_session_cancel_message(soup_session, msg,
					    SOUP_STATUS_CANCELLED);
		return;
	}

	s->base.ready = true;
	s->base.cond->broadcast();
	s->base.mutex->unlock();

	soup_message_body_set_accumulate(msg->response_body, false);
}

static void
input_soup_got_chunk(SoupMessage *msg, SoupBuffer *chunk, gpointer user_data)
{
	SoupInputStream *s = (SoupInputStream *)user_data;

	assert(msg == s->msg);

	const ScopeLock protect(*s->base.mutex);

	g_queue_push_tail(s->buffers, soup_buffer_copy(chunk));
	s->total_buffered += chunk->length;

	if (s->total_buffered >= SOUP_MAX_BUFFERED && !s->pause) {
		s->pause = true;
		soup_session_pause_message(soup_session, msg);
	}

	s->base.cond->broadcast();
	s->base.mutex->unlock();
}

static void
input_soup_got_body(G_GNUC_UNUSED SoupMessage *msg, gpointer user_data)
{
	SoupInputStream *s = (SoupInputStream *)user_data;

	assert(msg == s->msg);

	const ScopeLock protect(*s->base.mutex);

	s->base.ready = true;
	s->eof = true;
	s->alive = false;

	s->base.cond->broadcast();
	s->base.mutex->unlock();
}

static bool
input_soup_wait_data(SoupInputStream *s)
{
	while (true) {
		if (s->eof)
			return true;

		if (!s->alive)
			return false;

		if (!g_queue_is_empty(s->buffers))
			return true;

		assert(s->current_consumed == 0);

		s->base.cond->wait(*s->base.mutex);
	}
}

static gpointer
input_soup_queue(gpointer data)
{
	SoupInputStream *s = (SoupInputStream *)data;

	soup_session_queue_message(soup_session, s->msg,
				   input_soup_session_callback, s);

	return NULL;
}

static struct input_stream *
input_soup_open(const char *uri,
		Mutex &mutex, Cond &cond,
		G_GNUC_UNUSED GError **error_r)
{
	if (strncmp(uri, "http://", 7) != 0)
		return NULL;

	SoupInputStream *s = g_new(SoupInputStream, 1);
	input_stream_init(&s->base, &input_plugin_soup, uri,
			  mutex, cond);

	s->buffers = g_queue_new();
	s->current_consumed = 0;
	s->total_buffered = 0;

#if GCC_CHECK_VERSION(4,6)
#pragma GCC diagnostic push
	/* the libsoup macro SOUP_METHOD_GET discards the "const"
	   attribute of the g_intern_static_string() return value;
	   don't make the gcc warning fatal: */
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

	s->msg = soup_message_new(SOUP_METHOD_GET, uri);

#if GCC_CHECK_VERSION(4,6)
#pragma GCC diagnostic pop
#endif

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
	s->completed = false;
	s->postponed_error = NULL;

	io_thread_call(input_soup_queue, s);

	return &s->base;
}

static gpointer
input_soup_cancel(gpointer data)
{
	SoupInputStream *s = (SoupInputStream *)data;

	if (!s->completed)
		soup_session_cancel_message(soup_session, s->msg,
					    SOUP_STATUS_CANCELLED);

	return NULL;
}

static void
input_soup_close(struct input_stream *is)
{
	SoupInputStream *s = (SoupInputStream *)is;

		s->base.mutex->lock();

	if (!s->completed) {
		/* the messages's session callback hasn't been invoked
		   yet; cancel it and wait for completion */

		s->base.mutex->unlock();

		io_thread_call(input_soup_cancel, s);

		s->base.mutex->lock();
		while (!s->completed)
			s->base.cond->wait(*s->base.mutex);
	}

	s->base.mutex->unlock();

	SoupBuffer *buffer;
	while ((buffer = (SoupBuffer *)g_queue_pop_head(s->buffers)) != NULL)
		soup_buffer_free(buffer);
	g_queue_free(s->buffers);

	if (s->postponed_error != NULL)
		g_error_free(s->postponed_error);

	input_stream_deinit(&s->base);
	g_free(s);
}

static bool
input_soup_check(struct input_stream *is, GError **error_r)
{
	SoupInputStream *s = (SoupInputStream *)is;

	bool success = s->postponed_error == NULL;
	if (!success) {
		g_propagate_error(error_r, s->postponed_error);
		s->postponed_error = NULL;
	}

	return success;
}

static bool
input_soup_available(struct input_stream *is)
{
	SoupInputStream *s = (SoupInputStream *)is;

	return s->eof || !s->alive || !g_queue_is_empty(s->buffers);
}

static size_t
input_soup_read(struct input_stream *is, void *ptr, size_t size,
		G_GNUC_UNUSED GError **error_r)
{
	SoupInputStream *s = (SoupInputStream *)is;

	if (!input_soup_wait_data(s)) {
		assert(!s->alive);

		if (s->postponed_error != NULL) {
			g_propagate_error(error_r, s->postponed_error);
			s->postponed_error = NULL;
		} else
			g_set_error_literal(error_r, soup_quark(), 0,
					    "HTTP failure");
		return 0;
	}

	char *p0 = (char *)ptr, *p = p0, *p_end = p0 + size;

	while (p < p_end) {
		SoupBuffer *buffer = (SoupBuffer *)
			g_queue_pop_head(s->buffers);
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

	if (s->pause && s->total_buffered < SOUP_RESUME_AT) {
		s->pause = false;
		soup_session_unpause_message(soup_session, s->msg);
	}

	size_t nbytes = p - p0;
	s->base.offset += nbytes;

	return nbytes;
}

static bool
input_soup_eof(G_GNUC_UNUSED struct input_stream *is)
{
	SoupInputStream *s = (SoupInputStream *)is;

	return !s->alive && g_queue_is_empty(s->buffers);
}

const struct input_plugin input_plugin_soup = {
	"soup",
	input_soup_init,
	input_soup_finish,
	input_soup_open,
	input_soup_close,
	input_soup_check,
	nullptr,
	nullptr,
	input_soup_available,
	input_soup_read,
	input_soup_eof,
	nullptr,
};
