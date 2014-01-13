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

#ifndef MPD_OUTPUT_HTTPD_CLIENT_HXX
#define MPD_OUTPUT_HTTPD_CLIENT_HXX

#include "event/BufferedSocket.hxx"
#include "Compiler.h"

#include <queue>
#include <list>

#include <stddef.h>

class HttpdOutput;
class Page;

class HttpdClient final : BufferedSocket {
	/**
	 * The httpd output object this client is connected to.
	 */
	HttpdOutput &httpd;

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
	 * A queue of #Page objects to be sent to the client.
	 */
	std::queue<Page *, std::list<Page *>> pages;

	/**
	 * The sum of all page sizes in #pages.
	 */
	size_t queue_size;

	/**
	 * The #page which is currently being sent to the client.
	 */
	Page *current_page;

	/**
	 * The amount of bytes which were already sent from
	 * #current_page.
	 */
	size_t current_position;

	/**
	 * Is this a HEAD request?
	 */
	bool head_method;

	/**
         * If DLNA streaming was an option.
         */
	bool dlna_streaming_requested;

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
	unsigned metaint;

	/**
	 * The metadata as #Page which is currently being sent to the client.
	 */
	Page *metadata;

	/*
	 * The amount of bytes which were already sent from the metadata.
	 */
	size_t metadata_current_position;

	/**
	 * The amount of streaming data sent to the client
	 * since the last icy information was sent.
	 */
	unsigned metadata_fill;

public:
	/**
	 * @param httpd the HTTP output device
	 * @param fd the socket file descriptor
	 */
	HttpdClient(HttpdOutput &httpd, int _fd, EventLoop &_loop,
		    bool _metadata_supported);

	/**
	 * Note: this does not remove the client from the
	 * #HttpdOutput object.
	 */
	~HttpdClient();

	/**
	 * Frees the client and removes it from the server's client list.
	 */
	void Close();

	void LockClose();

	/**
	 * Clears the page queue.
	 */
	void CancelQueue();

	/**
	 * Handle a line of the HTTP request.
	 */
	bool HandleLine(const char *line);

	/**
	 * Switch the client to the "RESPONSE" state.
	 */
	void BeginResponse();

	/**
	 * Sends the status line and response headers to the client.
	 */
	bool SendResponse();

	gcc_pure
	ssize_t GetBytesTillMetaData() const;

	ssize_t TryWritePage(const Page &page, size_t position);
	ssize_t TryWritePageN(const Page &page, size_t position, ssize_t n);

	bool TryWrite();

	/**
	 * Appends a page to the client's queue.
	 */
	void PushPage(Page *page);

	/**
	 * Sends the passed metadata.
	 */
	void PushMetaData(Page *page);

private:
	void ClearQueue();

protected:
	virtual bool OnSocketReady(unsigned flags) override;
	virtual InputResult OnSocketInput(void *data, size_t length) override;
	virtual void OnSocketError(Error &&error) override;
	virtual void OnSocketClosed() override;
};

#endif
