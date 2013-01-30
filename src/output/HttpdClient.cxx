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
#include "HttpdClient.hxx"
#include "HttpdInternal.hxx"
#include "util/fifo_buffer.h"
#include "Page.hxx"
#include "IcyMetaDataServer.hxx"
#include "glib_socket.h"

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "httpd_output"

HttpdClient::~HttpdClient()
{
	if (state == RESPONSE) {
		if (write_source_id != 0)
			g_source_remove(write_source_id);

		if (current_page != nullptr)
			current_page->Unref();

		for (auto page : pages)
			page->Unref();
	} else
		fifo_buffer_free(input);

	if (metadata)
		metadata->Unref();

	g_source_remove(read_source_id);
	g_io_channel_unref(channel);
}

void
HttpdClient::Close()
{
	httpd->RemoveClient(*this);
}

void
HttpdClient::LockClose()
{
	const ScopeLock protect(httpd->mutex);
	Close();
}

void
HttpdClient::BeginResponse()
{
	assert(state != RESPONSE);

	state = RESPONSE;
	write_source_id = 0;
	current_page = nullptr;

	httpd->SendHeader(*this);
}

/**
 * Handle a line of the HTTP request.
 */
bool
HttpdClient::HandleLine(const char *line)
{
	assert(state != RESPONSE);

	if (state == REQUEST) {
		if (strncmp(line, "GET /", 5) != 0) {
			/* only GET is supported */
			g_warning("malformed request line from client");
			return false;
		}

		line = strchr(line + 5, ' ');
		if (line == nullptr || strncmp(line + 1, "HTTP/", 5) != 0) {
			/* HTTP/0.9 without request headers */
			BeginResponse();
			return true;
		}

		/* after the request line, request headers follow */
		state = HEADERS;
		return true;
	} else {
		if (*line == 0) {
			/* empty line: request is finished */
			BeginResponse();
			return true;
		}

		if (g_ascii_strncasecmp(line, "Icy-MetaData: 1", 15) == 0) {
			/* Send icy metadata */
			metadata_requested = metadata_supported;
			return true;
		}

		if (g_ascii_strncasecmp(line, "transferMode.dlna.org: Streaming", 32) == 0) {
			/* Send as dlna */
			dlna_streaming_requested = true;
			/* metadata is not supported by dlna streaming, so disable it */
			metadata_supported = false;
			metadata_requested = false;
			return true;
		}

		/* expect more request headers */
		return true;
	}
}

char *
HttpdClient::ReadLine()
{
	assert(state != RESPONSE);

	const ScopeLock protect(httpd->mutex);

	size_t length;
	const char *p = (const char *)fifo_buffer_read(input, &length);
	if (p == nullptr)
		/* empty input buffer */
		return nullptr;

	const char *newline = (const char *)memchr(p, '\n', length);
	if (newline == nullptr)
		/* incomplete line */
		return nullptr;

	char *line = g_strndup(p, newline - p);
	fifo_buffer_consume(input, newline - p + 1);

	/* remove trailing whitespace (e.g. '\r') */
	return g_strchomp(line);
}

/**
 * Sends the status line and response headers to the client.
 */
bool
HttpdClient::SendResponse()
{
	char buffer[1024];
	GError *error = nullptr;
	GIOStatus status;
	gsize bytes_written;

	assert(state == RESPONSE);

	if (dlna_streaming_requested) {
		g_snprintf(buffer, sizeof(buffer),
			   "HTTP/1.1 206 OK\r\n"
			   "Content-Type: %s\r\n"
			   "Content-Length: 10000\r\n"
			   "Content-RangeX: 0-1000000/1000000\r\n"
			   "transferMode.dlna.org: Streaming\r\n"
			   "Accept-Ranges: bytes\r\n"
			   "Connection: close\r\n"
			   "realTimeInfo.dlna.org: DLNA.ORG_TLAG=*\r\n"
			   "contentFeatures.dlna.org: DLNA.ORG_OP=01;DLNA.ORG_CI=0\r\n"
			   "\r\n",
			   httpd->content_type);

	} else if (metadata_requested) {
		gchar *metadata_header;

		metadata_header =
			icy_server_metadata_header(httpd->name, httpd->genre,
						   httpd->website,
						   httpd->content_type,
						   metaint);

		g_strlcpy(buffer, metadata_header, sizeof(buffer));

		g_free(metadata_header);

       } else { /* revert to a normal HTTP request */
		g_snprintf(buffer, sizeof(buffer),
			   "HTTP/1.1 200 OK\r\n"
			   "Content-Type: %s\r\n"
			   "Connection: close\r\n"
			   "Pragma: no-cache\r\n"
			   "Cache-Control: no-cache, no-store\r\n"
			   "\r\n",
			   httpd->content_type);
	}

	status = g_io_channel_write_chars(channel,
					  buffer, strlen(buffer),
					  &bytes_written, &error);

	switch (status) {
	case G_IO_STATUS_NORMAL:
	case G_IO_STATUS_AGAIN:
		return true;

	case G_IO_STATUS_EOF:
		/* client has disconnected */

		Close();
		return false;

	case G_IO_STATUS_ERROR:
		/* I/O error */

		g_warning("failed to write to client: %s", error->message);
		g_error_free(error);

		Close();
		return false;
	}

	/* unreachable */
	Close();
	return false;
}

bool
HttpdClient::Received()
{
	assert(state != RESPONSE);

	char *line;
	bool success;

	while ((line = ReadLine()) != nullptr) {
		success = HandleLine(line);
		g_free(line);
		if (!success) {
			assert(state != RESPONSE);
			return false;
		}

		if (state == RESPONSE) {
			if (!fifo_buffer_is_empty(input)) {
				g_warning("unexpected input from client");
				return false;
			}

			fifo_buffer_free(input);

			return SendResponse();
		}
	}

	return true;
}

bool
HttpdClient::Read()
{
	size_t max_length;
	GError *error = nullptr;
	GIOStatus status;
	gsize bytes_read;

	if (state == RESPONSE) {
		/* the client has already sent the request, and he
		   must not send more */
		char buffer[1];

		status = g_io_channel_read_chars(channel, buffer,
						 sizeof(buffer), &bytes_read,
						 nullptr);
		if (status == G_IO_STATUS_NORMAL)
			g_warning("unexpected input from client");

		return false;
	}

	char *p = (char *)fifo_buffer_write(input, &max_length);
	if (p == nullptr) {
		g_warning("buffer overflow");
		return false;
	}

	status = g_io_channel_read_chars(channel, p, max_length,
					 &bytes_read, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
		fifo_buffer_append(input, bytes_read);
		return Received();

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
	HttpdClient *client = (HttpdClient *)data;

	if (condition == G_IO_IN && client->Read()) {
		return true;
	} else {
		client->LockClose();
		return false;
	}
}

HttpdClient::HttpdClient(HttpdOutput *_httpd, int _fd,
			 bool _metadata_supported)
	:httpd(_httpd),
	 channel(g_io_channel_new_socket(_fd)),
	 input(fifo_buffer_new(4096)),
	 state(REQUEST),
	 dlna_streaming_requested(false),
	 metadata_supported(_metadata_supported),
	 metadata_requested(false), metadata_sent(true),
	 metaint(8192), /*TODO: just a std value */
	 metadata(nullptr),
	 metadata_current_position(0), metadata_fill(0)
{
	/* GLib is responsible for closing the file descriptor */
	g_io_channel_set_close_on_unref(channel, true);
	/* NULL encoding means the stream is binary safe */
	g_io_channel_set_encoding(channel, nullptr, nullptr);
	/* we prefer to do buffering */
	g_io_channel_set_buffered(channel, false);

	read_source_id = g_io_add_watch(channel,
					GIOCondition(G_IO_IN|G_IO_ERR|G_IO_HUP),
					httpd_client_in_event, this);
}

size_t
HttpdClient::GetQueueSize() const
{
	if (state != RESPONSE)
		return 0;

	size_t size = 0;
	for (auto page : pages)
		size += page->size;
	return size;
}

void
HttpdClient::CancelQueue()
{
	if (state != RESPONSE)
		return;

	for (auto page : pages)
		page->Unref();
	pages.clear();

	if (write_source_id != 0 && current_page == nullptr) {
		g_source_remove(write_source_id);
		write_source_id = 0;
	}
}

static GIOStatus
write_page_to_channel(GIOChannel *channel,
		      const Page &page, size_t position,
		      gsize *bytes_written_r, GError **error)
{
	assert(channel != nullptr);
	assert(position < page.size);

	return g_io_channel_write_chars(channel,
					(const gchar*)page.data + position,
					page.size - position,
					bytes_written_r, error);
}

static GIOStatus
write_n_bytes_to_channel(GIOChannel *channel, const Page &page,
			 size_t position, gint n,
			 gsize *bytes_written_r, GError **error)
{
	GIOStatus status;

	assert(channel != nullptr);
	assert(position < page.size);

	if (n == -1) {
		status =  write_page_to_channel (channel, page, position,
						 bytes_written_r, error);
	} else {
		status = g_io_channel_write_chars(channel,
						  (const gchar*)page.data + position,
						  n, bytes_written_r, error);
	}

	return status;
}

int
HttpdClient::GetBytesTillMetaData() const
{
	if (metadata_requested &&
	    current_page->size - current_position > metaint - metadata_fill)
		return metaint - metadata_fill;

	return -1;
}

inline bool
HttpdClient::Write()
{
	GError *error = nullptr;
	GIOStatus status;
	gsize bytes_written;

	const ScopeLock protect(httpd->mutex);

	assert(state == RESPONSE);

	if (write_source_id == 0)
		/* another thread has removed the event source while
		   this thread was waiting for httpd->mutex */
		return false;

	if (current_page == nullptr) {
		current_page = pages.front();
		pages.pop_front();
		current_position = 0;
	}

	const gint bytes_to_write = GetBytesTillMetaData();
	if (bytes_to_write == 0) {
		gint metadata_to_write;

		metadata_to_write = metadata_current_position;

		if (!metadata_sent) {
			status = write_page_to_channel(channel,
						       *metadata,
						       metadata_to_write,
						       &bytes_written, &error);

			metadata_current_position += bytes_written;

			if (metadata->size - metadata_current_position == 0) {
				metadata_fill = 0;
				metadata_current_position = 0;
				metadata_sent = true;
			}
		} else {
			guchar empty_data = 0;

			Page *empty_meta = Page::Copy(&empty_data, 1);

			status = write_page_to_channel(channel,
						       *empty_meta,
						       metadata_to_write,
						       &bytes_written, &error);

			metadata_current_position += bytes_written;

			if (empty_meta->size - metadata_current_position == 0) {
				metadata_fill = 0;
				metadata_current_position = 0;
			}

			empty_meta->Unref();
		}

		bytes_written = 0;
	} else {
		status = write_n_bytes_to_channel(channel, *current_page,
						  current_position, bytes_to_write,
						  &bytes_written, &error);
	}

	switch (status) {
	case G_IO_STATUS_NORMAL:
		current_position += bytes_written;
		assert(current_position <= current_page->size);

		if (metadata_requested)
			metadata_fill += bytes_written;

		if (current_position >= current_page->size) {
			current_page->Unref();
			current_page = nullptr;

			if (pages.empty()) {
				/* all pages are sent: remove the
				   event source */
				write_source_id = 0;

				return false;
			}
		}

		return true;

	case G_IO_STATUS_AGAIN:
		return true;

	case G_IO_STATUS_EOF:
		/* client has disconnected */

		Close();
		return false;

	case G_IO_STATUS_ERROR:
		/* I/O error */

		g_warning("failed to write to client: %s", error->message);
		g_error_free(error);

		Close();
		return false;
	}

	/* unreachable */
	Close();
	return false;
}

static gboolean
httpd_client_out_event(gcc_unused GIOChannel *source,
		       gcc_unused GIOCondition condition, gpointer data)
{
	assert(condition == G_IO_OUT);

	HttpdClient *client = (HttpdClient *)data;
	return client->Write();
}

void
HttpdClient::PushPage(Page *page)
{
	if (state != RESPONSE)
		/* the client is still writing the HTTP request */
		return;

	page->Ref();
	pages.push_back(page);

	if (write_source_id == 0)
		write_source_id = g_io_add_watch(channel, G_IO_OUT,
						 httpd_client_out_event,
						 this);
}

void
HttpdClient::PushMetaData(Page *page)
{
	if (metadata) {
		metadata->Unref();
		metadata = nullptr;
	}

	g_return_if_fail (page);

	page->Ref();
	metadata = page;
	metadata_sent = false;
}
