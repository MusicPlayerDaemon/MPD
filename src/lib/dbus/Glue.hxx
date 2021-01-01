/*
 * Copyright 2018-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef ODBUS_GLUE_HXX
#define ODBUS_GLUE_HXX

#include "Watch.hxx"

class EventLoop;

namespace ODBus {

/**
 * A class which manages the D-Bus client connection.
 */
class Glue final : ODBus::WatchManagerObserver {
	WatchManager watch;

public:
	explicit Glue(EventLoop &event_loop)
		:watch(event_loop, *this) {
		ConnectIndirect();
	}

	~Glue() noexcept {
		DisconnectIndirect();
	}

	auto &GetEventLoop() const noexcept {
		return watch.GetEventLoop();
	}

	Connection &GetConnection() noexcept {
		return watch.GetConnection();
	}

private:
	void ConnectIndirect();
	void DisconnectIndirect() noexcept;

	void Connect();
	void Disconnect() noexcept;

	/* virtual methods from class ODBus::WatchManagerObserver */
	void OnDBusClosed() noexcept override;
};

}

#endif
