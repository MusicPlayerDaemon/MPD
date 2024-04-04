// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Multi.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "event/DeferEvent.hxx"

class CurlSocket;
class CurlRequest;

/**
 * Manager for the global CURLM object.
 */
class CurlGlobal final {
	CurlMulti multi;

	DeferEvent defer_read_info;
	CoarseTimerEvent timeout_event;

public:
	explicit CurlGlobal(EventLoop &_loop);

	auto &GetEventLoop() const noexcept {
		return timeout_event.GetEventLoop();
	}

	void Add(CurlRequest &r);
	void Remove(CurlRequest &r) noexcept;

	void Assign(curl_socket_t fd, CurlSocket &cs) noexcept {
		curl_multi_assign(multi.Get(), fd, &cs);
	}

	void SocketAction(curl_socket_t fd, int ev_bitmask) noexcept;

	void InvalidateSockets() noexcept {
		SocketAction(CURL_SOCKET_TIMEOUT, 0);
	}

private:
	/**
	 * Check for finished HTTP responses.
	 *
	 * Runs in the I/O thread.  The caller must not hold locks.
	 */
	void ReadInfo() noexcept;

	void UpdateTimeout(long timeout_ms) noexcept;
	static int TimerFunction(CURLM *multi, long timeout_ms,
				 void *userp) noexcept;

	/* callback for #timeout_event */
	void OnTimeout() noexcept;
};
