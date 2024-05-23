// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "HttpdClient.hxx"
#include "HttpdInternal.hxx"
#include "util/AllocatedString.hxx"
#include "Page.hxx"
#include "IcyMetaDataServer.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/SpanCast.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"
#include "Log.hxx"

#include <fmt/core.h>

#include <cassert>

using std::string_view_literals::operator""sv;

HttpdClient::~HttpdClient() noexcept
{
	if (IsDefined())
		BufferedSocket::Close();
}

void
HttpdClient::Close() noexcept
{
	httpd.RemoveClient(*this);
}

void
HttpdClient::LockClose() noexcept
{
	const std::scoped_lock protect{httpd.mutex};
	Close();
}

void
HttpdClient::BeginResponse() noexcept
{
	assert(state != State::RESPONSE);

	state = State::RESPONSE;
	current_page = nullptr;

	if (!head_method)
		httpd.SendHeader(*this);
}

bool
HttpdClient::HandleLine(std::string_view line) noexcept
{
	assert(state != State::RESPONSE);

	if (state == State::REQUEST) {
		if (SkipPrefix(line, "HEAD /"sv)) {
			head_method = true;
		} else if (!SkipPrefix(line, "GET /"sv)) {
			/* only GET is supported */
			LogWarning(httpd_output_domain,
				   "malformed request line from client");
			return false;
		}

		const auto [uri, rest] = Split(line, ' ');

		/* blacklist some well-known request paths */
		if (uri == "favicon.ico"sv ||
		    uri == "robots.txt"sv ||
		    uri == "sitemap.xml"sv ||
		    uri.starts_with(".well-known/"sv)) {
			should_reject = true;
		}

		if (!rest.starts_with("HTTP/"sv)) {
			/* HTTP/0.9 without request headers */

			if (head_method)
				return false;

			BeginResponse();
			return true;
		}

		/* after the request line, request headers follow */
		state = State::HEADERS;
		return true;
	} else {
		if (line.empty()) {
			/* empty line: request is finished */

			BeginResponse();
			return true;
		}

		if (StringIsEqualIgnoreCase(line, "Icy-MetaData: 1"sv) ||
		    StringIsEqualIgnoreCase(line, "Icy-MetaData:1"sv)) {
			/* Send icy metadata */
			metadata_requested = metadata_supported;
			return true;
		}

		/* expect more request headers */
		return true;
	}
}

bool
HttpdClient::SendResponse() noexcept
{
	std::string allocated;
	std::string_view response;

	assert(state == State::RESPONSE);

	if (should_reject) {
		response =
			"HTTP/1.1 404 not found\r\n"
			"Content-Type: text/plain\r\n"
			"Connection: close\r\n"
			"\r\n"
			"404 not found"sv;
	} else if (metadata_requested) {
		allocated =
			icy_server_metadata_header(httpd.name, httpd.genre,
						   httpd.website,
						   httpd.content_type,
						   metaint);
		response = allocated;
	} else { /* revert to a normal HTTP request */
		allocated = fmt::format("HTTP/1.1 200 OK\r\n"
					"Content-Type: {}\r\n"
					"Connection: close\r\n"
					"Pragma: no-cache\r\n"
					"Cache-Control: no-cache, no-store\r\n"
					"Access-Control-Allow-Origin: *\r\n"
					"\r\n",
					httpd.content_type);
		response = allocated;
	}

	ssize_t nbytes = GetSocket().WriteNoWait(AsBytes(response));
	if (nbytes < 0) [[unlikely]] {
		const SocketErrorMessage msg;
		FmtWarning(httpd_output_domain,
			   "failed to write to client: {}",
			   (const char *)msg);
		LockClose();
		return false;
	}

	return true;
}

HttpdClient::HttpdClient(HttpdOutput &_httpd, UniqueSocketDescriptor _fd,
			 EventLoop &_loop,
			 bool _metadata_supported)
	:BufferedSocket(_fd.Release(), _loop),
	 httpd(_httpd),
	 metadata_supported(_metadata_supported)
{
}

void
HttpdClient::ClearQueue() noexcept
{
	assert(state == State::RESPONSE);

	while (!pages.empty()) {
#ifndef NDEBUG
		auto &page = pages.front();
		assert(queue_size >= page->size());
		queue_size -= page->size();
#endif

		pages.pop();
	}

	assert(queue_size == 0);
}

void
HttpdClient::CancelQueue() noexcept
{
	if (state != State::RESPONSE)
		return;

	ClearQueue();

	if (current_page == nullptr)
		event.CancelWrite();
}

ssize_t
HttpdClient::TryWritePage(const Page &page, size_t position) noexcept
{
	assert(position < page.size());

	return GetSocket().WriteNoWait(std::span<const std::byte>{page}.subspan(position));
}

ssize_t
HttpdClient::TryWritePageN(const Page &page,
			   size_t position, ssize_t n) noexcept
{
	return n >= 0
		? GetSocket().WriteNoWait({page.data() + position, (std::size_t)n})
		: TryWritePage(page, position);
}

ssize_t
HttpdClient::GetBytesTillMetaData() const noexcept
{
	if (metadata_requested &&
	    current_page->size() - current_position > metaint - metadata_fill)
		return metaint - metadata_fill;

	return -1;
}

inline bool
HttpdClient::TryWrite() noexcept
{
	const std::scoped_lock protect{httpd.mutex};

	assert(state == State::RESPONSE);

	if (current_page == nullptr) {
		if (pages.empty()) {
			/* another thread has removed the event source
			   while this thread was waiting for
			   httpd.mutex */
			event.CancelWrite();
			return true;
		}

		current_page = pages.front();
		pages.pop();
		current_position = 0;

		assert(queue_size >= current_page->size());
		queue_size -= current_page->size();
	}

	const ssize_t bytes_to_write = GetBytesTillMetaData();
	if (bytes_to_write == 0) {
		if (!metadata_sent) {
			ssize_t nbytes = TryWritePage(*metadata,
						      metadata_current_position);
			if (nbytes < 0) {
				auto e = GetSocketError();
				if (IsSocketErrorSendWouldBlock(e))
					return true;

				if (!IsSocketErrorClosed(e)) {
					SocketErrorMessage msg(e);
					FmtWarning(httpd_output_domain,
						   "failed to write to client: {}",
						   (const char *)msg);
				}

				Close();
				return false;
			}

			metadata_current_position += nbytes;

			if (metadata->size() - metadata_current_position == 0) {
				metadata_fill = 0;
				metadata_current_position = 0;
				metadata_sent = true;
			}
		} else {
			static constexpr std::byte empty_data[1]{};

			ssize_t nbytes = GetSocket().Write(empty_data);
			if (nbytes < 0) {
				auto e = GetSocketError();
				if (IsSocketErrorSendWouldBlock(e))
					return true;

				if (!IsSocketErrorClosed(e)) {
					SocketErrorMessage msg(e);
					FmtWarning(httpd_output_domain,
						   "failed to write to client: {}",
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
			if (IsSocketErrorSendWouldBlock(e))
				return true;

			if (!IsSocketErrorClosed(e)) {
				SocketErrorMessage msg(e);
				FmtWarning(httpd_output_domain,
					   "failed to write to client: {}",
					   (const char *)msg);
			}

			Close();
			return false;
		}

		current_position += nbytes;
		assert(current_position <= current_page->size());

		if (metadata_requested)
			metadata_fill += nbytes;

		if (current_position >= current_page->size()) {
			current_page.reset();

			if (pages.empty())
				/* all pages are sent: remove the
				   event source */
				event.CancelWrite();
		}
	}

	return true;
}

void
HttpdClient::PushPage(PagePtr page) noexcept
{
	if (state != State::RESPONSE)
		/* the client is still writing the HTTP request */
		return;

	if (queue_size > 256 * 1024) {
		LogDebug(httpd_output_domain,
			 "client is too slow, flushing its queue");
		ClearQueue();
	}

	queue_size += page->size();
	pages.emplace(std::move(page));

	event.ScheduleWrite();
}

void
HttpdClient::PushMetaData(PagePtr page) noexcept
{
	assert(page != nullptr);

	metadata = std::move(page);
	metadata_sent = false;
}

void
HttpdClient::OnSocketReady(unsigned flags) noexcept
{
	if (flags & SocketEvent::WRITE)
		if (!TryWrite())
			return;

	BufferedSocket::OnSocketReady(flags);
}

BufferedSocket::InputResult
HttpdClient::OnSocketInput(std::span<std::byte> _src) noexcept
{
	if (state == State::RESPONSE) {
		LogWarning(httpd_output_domain,
			   "unexpected input from client");
		LockClose();
		return InputResult::CLOSED;
	}

	const auto src = ToStringView(_src);
	auto [line, rest] = Split(src, '\n');
	if (rest.data() == nullptr)
		return InputResult::MORE;

	ConsumeInput(line.size() + 1);

	if (line.ends_with('\r'))
		line.remove_suffix(1);

	if (!HandleLine(line)) {
		LockClose();
		return InputResult::CLOSED;
	}

	if (state == State::RESPONSE) {
		if (!SendResponse())
			return InputResult::CLOSED;

		if (head_method || should_reject) {
			LockClose();
			return InputResult::CLOSED;
		}
	}

	return InputResult::AGAIN;
}

void
HttpdClient::OnSocketError(std::exception_ptr ep) noexcept
{
	LogError(ep);
	LockClose();
}

void
HttpdClient::OnSocketClosed() noexcept
{
	LockClose();
}
