// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Page.hxx"
#include "event/BufferedSocket.hxx"
#include "util/IntrusiveList.hxx"

#include <cstddef>
#include <list>
#include <queue>
#include <string_view>

class UniqueSocketDescriptor;
class HttpdOutput;

class HttpdClient final
	: BufferedSocket,
	  public IntrusiveListHook<>
{
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
	static constexpr std::size_t metaint = 8192;

	/**
	 * The metadata as #Page which is currently being sent to the client.
	 */
	PagePtr metadata;

	/**
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
	bool HandleLine(std::string_view line) noexcept;

	/**
	 * Switch the client to #State::RESPONSE.
	 */
	void BeginResponse() noexcept;

	/**
	 * Sends the status line and response headers to the client.
	 */
	bool SendResponse() noexcept;

	[[gnu::pure]]
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

	InputResult OnSocketInput(std::span<std::byte> src) noexcept override;
	void OnSocketError(std::exception_ptr ep) noexcept override;
	void OnSocketClosed() noexcept override;
};
