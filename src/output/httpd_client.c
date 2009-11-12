/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "httpd_client.h"
#include "httpd_internal.h"
#include "fifo_buffer.h"
#include "page.h"
#include "icy_server.h"
#include "glib_compat.h"

#include <stdbool.h>
#include <assert.h>
#include <string.h>

struct httpd_client {
	/**
	 * The httpd output object this client is connected to.
	 */
	struct httpd_output *httpd;

	/**
	 * The TCP socket.
	 */
	GIOChannel *channel;

	/**
	 * The GLib main loop source id for reading from the socket,
	 * and to detect errors.
	 */
	guint read_source_id;

	/**
	 * The GLib main loop source id for writing to the socket.  If
	 * 0, then there is no event source currently (because there
	 * are no queued pages).
	 */
	guint write_source_id;

	/**
	 * For buffered reading.  This pointer is only valid while the
	 * HTTP request is read.
	 */
	struct fifo_buffer *input;

	/**
	 * The current state of the client.
	 */
	enum {
		/** reading the request line */
		REQUEST,

		/** reading the request headers */
		HEADERS,

		/** sending the HTTP response */
		RESPONSE,
	} state;

	/**
	 * A queue of #page objects to be sent to the client.
	 */
	GQueue *pages;

	/**
	 * The #page which is currently being sent to the client.
	 */
	struct page *current_page;

	/**
	 * The amount of bytes which were already sent from
	 * #current_page.
	 */
	size_t current_position;

	/* ICY */

	/**
	 * Do we support sending Icy-Metadata to the client?  This is
	 * disabled if the httpd audio output uses encoder tags.
	 */
	bool metadata_supported;

	/**
	 * If we should sent icy metadata.
	 */
	bool metadata_requested;

	/**
	 * If the current metadata was already sent to the client.
	 */
	bool metadata_sent;

	/**
	 * The amount of streaming data between each metadata block
	 */
	guint metaint;

	/**
	 * The metadata as #page which is currently being sent to the client.
	 */
	struct page *metadata;

	/*
	 * The amount of bytes which were already sent from the metadata.
	 */
	size_t metadata_current_position;

	/**
	 * The amount of streaming data sent to the client
	 * since the last icy information was sent.
	 */
	guint metadata_fill;
};

static void
httpd_client_unref_page(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct page *page = data;

	page_unref(page);
}

void
httpd_client_free(struct httpd_client *client)
{
	if (client->state == RESPONSE) {
		if (client->write_source_id != 0)
			g_source_remove(client->write_source_id);

		if (client->current_page != NULL)
			page_unref(client->current_page);

		g_queue_foreach(client->pages, httpd_client_unref_page, NULL);
		g_queue_free(client->pages);
	} else
		fifo_buffer_free(client->input);

	if (client->metadata)
		page_unref (client->metadata);

	g_source_remove(client->read_source_id);
	g_io_channel_unref(client->channel);
	g_free(client);
}

/**
 * Frees the client and removes it from the server's client list.
 */
static void
httpd_client_close(struct httpd_client *client)
{
	httpd_output_remove_client(client->httpd, client);
	httpd_client_free(client);
}

/**
 * Switch the client to the "RESPONSE" state.
 */
static void
httpd_client_begin_response(struct httpd_client *client)
{
	client->state = RESPONSE;
	client->write_source_id = 0;
	client->pages = g_queue_new();
	client->current_page = NULL;

	httpd_output_send_header(client->httpd, client);
}

/**
 * Handle a line of the HTTP request.
 */
static bool
httpd_client_handle_line(struct httpd_client *client, const char *line)
{
	assert(client->state != RESPONSE);

	if (client->state == REQUEST) {
		if (strncmp(line, "GET /", 5) != 0) {
			/* only GET is supported */
			g_warning("malformed request line from client");
			return false;
		}

		line = strchr(line + 5, ' ');
		if (line == NULL || strncmp(line + 1, "HTTP/", 5) != 0) {
			/* HTTP/0.9 without request headers */
			httpd_client_begin_response(client);
			return true;
		}

		/* after the request line, request headers follow */
		client->state = HEADERS;
		return true;
	} else {
		if (*line == 0) {
			/* empty line: request is finished */
			httpd_client_begin_response(client);
			return true;
		}

		if (g_ascii_strncasecmp(line, "Icy-MetaData: 1", 15) == 0) {
			/* Send icy metadata */
			client->metadata_requested =
				client->metadata_supported;
			return true;
		}

		/* expect more request headers */
		return true;
	}
}

/**
 * Check if a complete line of input is present in the input buffer,
 * and duplicates it.  It is removed from the input buffer.  The
 * return value has to be freed with g_free().
 */
static char *
httpd_client_read_line(struct httpd_client *client)
{
	const char *p, *newline;
	size_t length;
	char *line;

	p = fifo_buffer_read(client->input, &length);
	if (p == NULL)
		/* empty input buffer */
		return NULL;

	newline = memchr(p, '\n', length);
	if (newline == NULL)
		/* incomplete line */
		return NULL;

	line = g_strndup(p, newline - p);
	fifo_buffer_consume(client->input, newline - p + 1);

	/* remove trailing whitespace (e.g. '\r') */
	return g_strchomp(line);
}

/**
 * Sends the status line and response headers to the client.
 */
static bool
httpd_client_send_response(struct httpd_client *client)
{
	char buffer[1024];
	GError *error = NULL;
	GIOStatus status;
	gsize bytes_written;

	assert(client->state == RESPONSE);

	if (!client->metadata_requested) {
		g_snprintf(buffer, sizeof(buffer),
			   "HTTP/1.1 200 OK\r\n"
			   "Content-Type: %s\r\n"
			   "Connection: close\r\n"
			   "Pragma: no-cache\r\n"
			   "Cache-Control: no-cache, no-store\r\n"
			   "\r\n",
			   client->httpd->content_type);
	} else {
		gchar *metadata_header;

		metadata_header = icy_server_metadata_header("Add config information here!", /* TODO */
							     "Add config information here!", /* TODO */
							     "Add config information here!", /* TODO */
							     client->httpd->content_type,
							     client->metaint);

		g_strlcpy(buffer, metadata_header, sizeof(buffer));

		g_free(metadata_header);
	}

	status = g_io_channel_write_chars(client->channel,
					  buffer, strlen(buffer),
					  &bytes_written, &error);

	switch (status) {
	case G_IO_STATUS_NORMAL:
	case G_IO_STATUS_AGAIN:
		return true;

	case G_IO_STATUS_EOF:
		/* client has disconnected */

		httpd_client_close(client);
		return false;

	case G_IO_STATUS_ERROR:
		/* I/O error */

		g_warning("failed to write to client: %s", error->message);
		g_error_free(error);

		httpd_client_close(client);
		return false;
	}

	/* unreachable */
	httpd_client_close(client);
	return false;
}

/**
 * Data has been received from the client and it is appended to the
 * input buffer.
 */
static bool
httpd_client_received(struct httpd_client *client)
{
	char *line;
	bool success;

	while ((line = httpd_client_read_line(client)) != NULL) {
		success = httpd_client_handle_line(client, line);
		g_free(line);
		if (!success)
			return false;

		if (client->state == RESPONSE) {
			if (!fifo_buffer_is_empty(client->input)) {
				g_warning("unexpected input from client");
				return false;
			}

			fifo_buffer_free(client->input);

			return httpd_client_send_response(client);
		}
	}

	return true;
}

static bool
httpd_client_read(struct httpd_client *client)
{
	char *p;
	size_t max_length;
	GError *error = NULL;
	GIOStatus status;
	gsize bytes_read;

	if (client->state == RESPONSE) {
		/* the client has already sent the request, and he
		   must not send more */
		g_warning("unexpected input from client");
		return false;
	}

	p = fifo_buffer_write(client->input, &max_length);
	if (p == NULL) {
		g_warning("buffer overflow");
		return false;
	}

	status = g_io_channel_read_chars(client->channel, p, max_length,
					 &bytes_read, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
		fifo_buffer_append(client->input, bytes_read);
		return httpd_client_received(client);

	case G_IO_STATUS_AGAIN:
		/* try again later, after select() */
		return true;

	case G_IO_STATUS_EOF:
		/* peer disconnected */
		return false;

	case G_IO_STATUS_ERROR:
		/* I/O error */
		g_warning("failed to read from client: %s",
			  error->message);
		g_error_free(error);
		return false;
	}

	/* unreachable */
	return false;
}

static gboolean
httpd_client_in_event(G_GNUC_UNUSED GIOChannel *source, GIOCondition condition,
		      gpointer data)
{
	struct httpd_client *client = data;
	struct httpd_output *httpd = client->httpd;
	bool ret;

	g_mutex_lock(httpd->mutex);

	if (condition == G_IO_IN && httpd_client_read(client)) {
		ret = true;
	} else {
		httpd_client_close(client);
		ret = false;
	}

	g_mutex_unlock(httpd->mutex);

	return ret;
}

struct httpd_client *
httpd_client_new(struct httpd_output *httpd, int fd, bool metadata_supported)
{
	struct httpd_client *client = g_new(struct httpd_client, 1);

	client->httpd = httpd;

#ifndef G_OS_WIN32
	client->channel = g_io_channel_unix_new(fd);
#else
	client->channel = g_io_channel_win32_new_socket(fd);
#endif

	/* GLib is responsible for closing the file descriptor */
	g_io_channel_set_close_on_unref(client->channel, true);
	/* NULL encoding means the stream is binary safe */
	g_io_channel_set_encoding(client->channel, NULL, NULL);
	/* we prefer to do buffering */
	g_io_channel_set_buffered(client->channel, false);

	client->read_source_id = g_io_add_watch(client->channel,
						G_IO_IN|G_IO_ERR|G_IO_HUP,
						httpd_client_in_event, client);

	client->input = fifo_buffer_new(4096);
	client->state = REQUEST;

	client->metadata_supported = metadata_supported;
	client->metadata_requested = false;
	client->metadata_sent = true;
	client->metaint = 8192; /*TODO: just a std value */
	client->metadata = NULL;
	client->metadata_current_position = 0;
	client->metadata_fill = 0;

	return client;
}

static void
httpd_client_add_page_size(gpointer data, gpointer user_data)
{
	struct page *page = data;
	size_t *size = user_data;

	*size += page->size;
}

size_t
httpd_client_queue_size(const struct httpd_client *client)
{
	size_t size = 0;

	if (client->state != RESPONSE)
		return 0;

	g_queue_foreach(client->pages, httpd_client_add_page_size, &size);
	return size;
}

void
httpd_client_cancel(struct httpd_client *client)
{
	if (client->state != RESPONSE)
		return;

	g_queue_foreach(client->pages, httpd_client_unref_page, NULL);
	g_queue_clear(client->pages);

	if (client->write_source_id != 0 && client->current_page == NULL) {
		g_source_remove(client->write_source_id);
		client->write_source_id = 0;
	}
}

static GIOStatus
write_page_to_channel(GIOChannel *channel,
		      const struct page *page, size_t position,
		      gsize *bytes_written_r, GError **error)
{
	assert(channel != NULL);
	assert(page != NULL);
	assert(position < page->size);

	return g_io_channel_write_chars(channel,
					(const gchar*)page->data + position,
					page->size - position,
					bytes_written_r, error);
}

static GIOStatus
write_n_bytes_to_channel(GIOChannel *channel, const struct page *page,
			 size_t position, gint n,
			 gsize *bytes_written_r, GError **error)
{
	GIOStatus status;

	assert(channel != NULL);
	assert(page != NULL);
	assert(position < page->size);

	if (n == -1) {
		status =  write_page_to_channel (channel, page, position,
						 bytes_written_r, error);
	} else {
		status = g_io_channel_write_chars(channel,
						  (const gchar*)page->data + position,
						  n, bytes_written_r, error);
	}

	return status;
}

static gint
bytes_left_till_metadata (struct httpd_client *client)
{
	assert(client != NULL);

	if (client->metadata_requested &&
	    client->current_page->size - client->current_position
	    > client->metaint - client->metadata_fill)
		return client->metaint - client->metadata_fill;

	return -1;
}

static gboolean
httpd_client_out_event(GIOChannel *source,
		       G_GNUC_UNUSED GIOCondition condition, gpointer data)
{
	struct httpd_client *client = data;
	struct httpd_output *httpd = client->httpd;
	GError *error = NULL;
	GIOStatus status;
	gsize bytes_written;
	gint bytes_to_write;

	g_mutex_lock(httpd->mutex);

	assert(condition == G_IO_OUT);
	assert(client->state == RESPONSE);

	if (client->write_source_id == 0) {
		/* another thread has removed the event source while
		   this thread was waiting for httpd->mutex */
		g_mutex_unlock(httpd->mutex);
		return false;
	}

	if (client->current_page == NULL) {
		client->current_page = g_queue_pop_head(client->pages);
		client->current_position = 0;
	}

	bytes_to_write = bytes_left_till_metadata(client);

	if (bytes_to_write == 0) {
		gint metadata_to_write;

		metadata_to_write = client->metadata_current_position;

		if (!client->metadata_sent) {
			status = write_page_to_channel(source,
						       client->metadata,
						       metadata_to_write,
						       &bytes_written, &error);

			client->metadata_current_position += bytes_written;

			if (client->metadata->size
			    - client->metadata_current_position == 0) {
				client->metadata_fill = 0;
				client->metadata_current_position = 0;
				client->metadata_sent = true;
			}
		} else {
			struct page *empty_meta;
			guchar empty_data = 0;

			empty_meta = page_new_copy(&empty_data, 1);

			status = write_page_to_channel(source,
						       empty_meta,
						       metadata_to_write,
						       &bytes_written, &error);

			client->metadata_current_position += bytes_written;

			if (empty_meta->size
			    - client->metadata_current_position == 0) {
				client->metadata_fill = 0;
				client->metadata_current_position = 0;
			}
		}

		bytes_written = 0;
	} else {
		status = write_n_bytes_to_channel(source, client->current_page,
						  client->current_position, bytes_to_write,
						  &bytes_written, &error);
	}

	switch (status) {
	case G_IO_STATUS_NORMAL:
		client->current_position += bytes_written;
		assert(client->current_position <= client->current_page->size);

		if (client->metadata_requested)
			client->metadata_fill += bytes_written;

		if (client->current_position >= client->current_page->size) {
			page_unref(client->current_page);
			client->current_page = NULL;

			if (g_queue_is_empty(client->pages)) {
				/* all pages are sent: remove the
				   event source */
				client->write_source_id = 0;

				g_mutex_unlock(httpd->mutex);
				return false;
			}
		}

		g_mutex_unlock(httpd->mutex);
		return true;

	case G_IO_STATUS_AGAIN:
		g_mutex_unlock(httpd->mutex);
		return true;

	case G_IO_STATUS_EOF:
		/* client has disconnected */

		httpd_client_close(client);
		g_mutex_unlock(httpd->mutex);
		return false;

	case G_IO_STATUS_ERROR:
		/* I/O error */

		g_warning("failed to write to client: %s", error->message);
		g_error_free(error);

		httpd_client_close(client);
		g_mutex_unlock(httpd->mutex);
		return false;
	}

	/* unreachable */
	httpd_client_close(client);
	g_mutex_unlock(httpd->mutex);
	return false;
}

void
httpd_client_send(struct httpd_client *client, struct page *page)
{
	if (client->state != RESPONSE)
		/* the client is still writing the HTTP request */
		return;

	page_ref(page);
	g_queue_push_tail(client->pages, page);

	if (client->write_source_id == 0)
		client->write_source_id =
			g_io_add_watch(client->channel, G_IO_OUT,
				       httpd_client_out_event, client);
}

void
httpd_client_send_metadata(struct httpd_client *client, struct page *page)
{
	if (client->metadata) {
		page_unref(client->metadata);
		client->metadata = NULL;
	}

	g_return_if_fail (page);

	page_ref(page);
	client->metadata = page;
	client->metadata_sent = false;
}
