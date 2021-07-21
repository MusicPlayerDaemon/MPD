/*
 * Copyright 2008-2020 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CURL_GLOBAL_HXX
#define CURL_GLOBAL_HXX

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

#endif
