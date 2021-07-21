/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Page.hxx"
#include "event/BufferedSocket.hxx"
#include "util/Compiler.h"

#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list_hook.hpp>

#include <cstddef>
#include <list>
#include <queue>

class UniqueSocketDescriptor;
class HttpdOutput;

class HttpdClient final
	: BufferedSocket,
	  public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> {
	/**
	 * The httpd output object this client is connected to.
	 */
	HttpdOutput &httpd;

	/**
	 * The current state of the client.
	 */
	enum class State {
		/** reading the request line */
		REQUEST,

		/** reading the request headers */
		HEADERS,

		/** sending the HTTP response */
		RESPONSE,
	} state = State::REQUEST;

	/**
	 * A queue of #Page objects to be sent to the client.
	 */
	std::queue<PagePtr, std::list<PagePtr>> pages;

	/**
	 * The sum of all page sizes in #pages.
	 */
	size_t queue_size = 0;

	/**
	 * The #page which is currently being sent to the client.
	 */
	PagePtr current_page;

	/**
	 * The amount of bytes which were already sent from
	 * #current_page.
	 */
	size_t current_position;

	/**
	 * Is this a HEAD request?
	 */
	bool head_method = false;

	/**
	 * Should we reject this request?
	 */
	bool should_reject = false;

	/* ICY */

	/**
	 * Do we support sending Icy-Metadata to the client?  This is
	 * disabled if the httpd audio output uses encoder tags.
	 */
	bool metadata_supported;

	/**
	 * If we should sent icy metadata.
	 */
	bool metadata_requested = false;

	/**
	 * If the current metadata was already sent to the client.
	 *
	 * Initialized to `true` because there is no metadata #Page
	 * pending to be sent.
	 */
	bool metadata_sent = true;

	/**
	 * The amount of streaming data between each metadata block
	 */
	unsigned metaint = 8192; /*TODO: just a std value */

	/**
	 * The metadata as #Page which is currently being sent to the client.
	 */
	PagePtr metadata;

	/*
	 * The amount of bytes which were already sent from the metadata.
	 */
	size_t metadata_current_position = 0;

	/**
	 * The amount of streaming data sent to the client
	 * since the last icy information was sent.
	 */
	unsigned metadata_fill = 0;

public:
	/**
	 * @param httpd the HTTP output device
	 * @param _fd the socket file descriptor
	 */
	HttpdClient(HttpdOutput &httpd, UniqueSocketDescriptor _fd,
		    EventLoop &_loop,
		    bool _metadata_supported);

	/**
	 * Note: this does not remove the client from the
	 * #HttpdOutput object.
	 */
	~HttpdClient() noexcept;

	/**
	 * Frees the client and removes it from the server's client list.
	 *
	 * Caller must lock the mutex.
	 */
	void Close() noexcept;

	void LockClose() noexcept;

	/**
	 * Clears the page queue.
	 */
	void CancelQueue() noexcept;

	/**
	 * Handle a line of the HTTP request.
	 */
	bool HandleLine(const char *line) noexcept;

	/**
	 * Switch the client to #State::RESPONSE.
	 */
	void BeginResponse() noexcept;

	/**
	 * Sends the status line and response headers to the client.
	 */
	bool SendResponse() noexcept;

	gcc_pure
	ssize_t GetBytesTillMetaData() const noexcept;

	ssize_t TryWritePage(const Page &page, size_t position) noexcept;
	ssize_t TryWritePageN(const Page &page,
			      size_t position, ssize_t n) noexcept;

	bool TryWrite() noexcept;

	/**
	 * Appends a page to the client's queue.
	 */
	void PushPage(PagePtr page) noexcept;

	/**
	 * Sends the passed metadata.
	 */
	void PushMetaData(PagePtr page) noexcept;

private:
	void ClearQueue() noexcept;

protected:
	/* virtual methods from class BufferedSocket */
	void OnSocketReady(unsigned flags) noexcept override;

	InputResult OnSocketInput(void *data, size_t length) noexcept override;
	void OnSocketError(std::exception_ptr ep) noexcept override;
	void OnSocketClosed() noexcept override;
};

#endif
