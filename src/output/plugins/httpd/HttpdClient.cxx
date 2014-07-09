/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "util/ASCII.hxx"
#include "Page.hxx"
#include "IcyMetaDataServer.hxx"
#include "system/SocketError.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>
#include <stdio.h>

HttpdClient::~HttpdClient()
{
	if (state == RESPONSE) {
		if (current_page != nullptr)
			current_page->Unref();

		ClearQueue();
	}

	if (metadata)
		metadata->Unref();

	if (IsDefined())
		BufferedSocket::Close();
}

void
HttpdClient::Close()
{
	httpd.RemoveClient(*this);
}

void
HttpdClient::LockClose()
{
	const ScopeLock protect(httpd.mutex);
	Close();
}

void
HttpdClient::BeginResponse()
{
	assert(state != RESPONSE);

	state = RESPONSE;
	current_page = nullptr;

	if (!head_method)
		httpd.SendHeader(*this);
}

/**
 * Handle a line of the HTTP request.
 */
bool
HttpdClient::HandleLine(const char *line)
{
	assert(state != RESPONSE);

	if (state == REQUEST) {
		if (memcmp(line, "HEAD /", 6) == 0) {
			line += 6;
			head_method = true;
		} else if (memcmp(line, "GET /", 5) == 0) {
			line += 5;
		} else {
			/* only GET is supported */
			LogWarning(httpd_output_domain,
				   "malformed request line from client");
			return false;
		}

		line = strchr(line, ' ');
		if (line == nullptr || memcmp(line + 1, "HTTP/", 5) != 0) {
			/* HTTP/0.9 without request headers */

			if (head_method)
				return false;

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

		if (StringEqualsCaseASCII(line, "Icy-MetaData: 1", 15) ||
		    StringEqualsCaseASCII(line, "Icy-MetaData:1", 14)) {
			/* Send icy metadata */
			metadata_requested = metadata_supported;
			return true;
		}

		if (StringEqualsCaseASCII(line, "transferMode.dlna.org: Streaming", 32)) {
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

/**
 * Sends the status line and response headers to the client.
 */
bool
HttpdClient::SendResponse()
{
	char buffer[1024], *allocated = nullptr;
	const char *response;

	assert(state == RESPONSE);

	if (dlna_streaming_requested) {
		snprintf(buffer, sizeof(buffer),
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
			 httpd.content_type);
		response = buffer;

	} else if (metadata_requested) {
		response = allocated =
			icy_server_metadata_header(httpd.name, httpd.genre,
						   httpd.website,
						   httpd.content_type,
						   metaint);
       } else { /* revert to a normal HTTP request */
		snprintf(buffer, sizeof(buffer),
			 "HTTP/1.1 200 OK\r\n"
			 "Content-Type: %s\r\n"
			 "Connection: close\r\n"
			 "Pragma: no-cache\r\n"
			 "Cache-Control: no-cache, no-store\r\n"
			 "\r\n",
			 httpd.content_type);
		response = buffer;
	}

	ssize_t nbytes = SocketMonitor::Write(response, strlen(response));
	delete[] allocated;
	if (gcc_unlikely(nbytes < 0)) {
		const SocketErrorMessage msg;
		FormatWarning(httpd_output_domain,
			      "failed to write to client: %s",
			      (const char *)msg);
		Close();
		return false;
	}

	return true;
}

HttpdClient::HttpdClient(HttpdOutput &_httpd, int _fd, EventLoop &_loop,
			 bool _metadata_supported)
	:BufferedSocket(_fd, _loop),
	 httpd(_httpd),
	 state(REQUEST),
	 queue_size(0),
	 head_method(false),
	 dlna_streaming_requested(false),
	 metadata_supported(_metadata_supported),
	 metadata_requested(false), metadata_sent(true),
	 metaint(8192), /*TODO: just a std value */
	 metadata(nullptr),
	 metadata_current_position(0), metadata_fill(0)
{
}

void
HttpdClient::ClearQueue()
{
	assert(state == RESPONSE);

	while (!pages.empty()) {
		Page *page = pages.front();
		pages.pop();

#ifndef NDEBUG
		assert(queue_size >= page->size);
		queue_size -= page->size;
#endif

		page->Unref();
	}

	assert(queue_size == 0);
}

void
HttpdClient::CancelQueue()
{
	if (state != RESPONSE)
		return;

	ClearQueue();

	if (current_page == nullptr)
		CancelWrite();
}

ssize_t
HttpdClient::TryWritePage(const Page &page, size_t position)
{
	assert(position < page.size);

	return Write(page.data + position, page.size - position);
}

ssize_t
HttpdClient::TryWritePageN(const Page &page, size_t position, ssize_t n)
{
	return n >= 0
		? Write(page.data + position, n)
		: TryWritePage(page, position);
}

ssize_t
HttpdClient::GetBytesTillMetaData() const
{
	if (metadata_requested &&
	    current_page->size - current_position > metaint - metadata_fill)
		return metaint - metadata_fill;

	return -1;
}

inline bool
HttpdClient::TryWrite()
{
	const ScopeLock protect(httpd.mutex);

	assert(state == RESPONSE);

	if (current_page == nullptr) {
		if (pages.empty()) {
			/* another thread has removed the event source
			   while this thread was waiting for
			   httpd.mutex */
			CancelWrite();
			return true;
		}

		current_page = pages.front();
		pages.pop();
		current_position = 0;

		assert(queue_size >= current_page->size);
		queue_size -= current_page->size;
	}

	const ssize_t bytes_to_write = GetBytesTillMetaData();
	if (bytes_to_write == 0) {
		if (!metadata_sent) {
			ssize_t nbytes = TryWritePage(*metadata,
						      metadata_current_position);
			if (nbytes < 0) {
				auto e = GetSocketError();
				if (IsSocketErrorAgain(e))
					return true;

				if (!IsSocketErrorClosed(e)) {
					SocketErrorMessage msg(e);
					FormatWarning(httpd_output_domain,
						      "failed to write to client: %s",
						      (const char *)msg);
				}

				Close();
				return false;
			}

			metadata_current_position += nbytes;

			if (metadata->size - metadata_current_position == 0) {
				metadata_fill = 0;
				metadata_current_position = 0;
				metadata_sent = true;
			}
		} else {
			char empty_data = 0;

			ssize_t nbytes = Write(&empty_data, 1);
			if (nbytes < 0) {
				auto e = GetSocketError();
				if (IsSocketErrorAgain(e))
					return true;

				if (!IsSocketErrorClosed(e)) {
					SocketErrorMessage msg(e);
					FormatWarning(httpd_output_domain,
						      "failed to write to client: %s",
						      (const char *)msg);
				}

				Close();
				return false;
			}

			metadata_fill = 0;
			metadata_current_position = 0;
		}
	} else {
		ssize_t nbytes =
			TryWritePageN(*current_page, current_position,
				      bytes_to_write);
		if (nbytes < 0) {
			auto e = GetSocketError();
			if (IsSocketErrorAgain(e))
				return true;

			if (!IsSocketErrorClosed(e)) {
				SocketErrorMessage msg(e);
				FormatWarning(httpd_output_domain,
					      "failed to write to client: %s",
					      (const char *)msg);
			}

			Close();
			return false;
		}

		current_position += nbytes;
		assert(current_position <= current_page->size);

		if (metadata_requested)
			metadata_fill += nbytes;

		if (current_position >= current_page->size) {
			current_page->Unref();
			current_page = nullptr;

			if (pages.empty())
				/* all pages are sent: remove the
				   event source */
				CancelWrite();
		}
	}

	return true;
}

void
HttpdClient::PushPage(Page *page)
{
	if (state != RESPONSE)
		/* the client is still writing the HTTP request */
		return;

	if (queue_size > 256 * 1024) {
		FormatDebug(httpd_output_domain,
			    "client is too slow, flushing its queue");
		ClearQueue();
	}

	page->Ref();
	pages.push(page);
	queue_size += page->size;

	ScheduleWrite();
}

void
HttpdClient::PushMetaData(Page *page)
{
	assert(page != nullptr);

	if (metadata) {
		metadata->Unref();
		metadata = nullptr;
	}

	page->Ref();
	metadata = page;
	metadata_sent = false;
}

bool
HttpdClient::OnSocketReady(unsigned flags)
{
	if (!BufferedSocket::OnSocketReady(flags))
		return false;

	if (flags & WRITE)
		if (!TryWrite())
			return false;

	return true;
}

BufferedSocket::InputResult
HttpdClient::OnSocketInput(void *data, size_t length)
{
	if (state == RESPONSE) {
		LogWarning(httpd_output_domain,
			   "unexpected input from client");
		LockClose();
		return InputResult::CLOSED;
	}

	char *line = (char *)data;
	char *newline = (char *)memchr(line, '\n', length);
	if (newline == nullptr)
		return InputResult::MORE;

	ConsumeInput(newline + 1 - line);

	if (newline > line && newline[-1] == '\r')
		--newline;

	/* terminate the string at the end of the line */
	*newline = 0;

	if (!HandleLine(line)) {
		LockClose();
		return InputResult::CLOSED;
	}

	if (state == RESPONSE) {
		if (!SendResponse())
			return InputResult::CLOSED;

		if (head_method) {
			LockClose();
			return InputResult::CLOSED;
		}
	}

	return InputResult::AGAIN;
}

void
HttpdClient::OnSocketError(Error &&error)
{
	LogError(error);
}

void
HttpdClient::OnSocketClosed()
{
	LockClose();
}
