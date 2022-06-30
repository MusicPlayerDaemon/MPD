/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#pragma once

#include "Poll.hxx"
#include "event/CoarseTimerEvent.hxx"

#include <avahi-client/client.h>

#include <forward_list>

class EventLoop;

namespace Avahi {

class ErrorHandler;
class ConnectionListener;

class Client final {
	ErrorHandler &error_handler;

	CoarseTimerEvent reconnect_timer;

	Poll poll;

	AvahiClient *client = nullptr;

	std::forward_list<ConnectionListener *> listeners;

public:
	Client(EventLoop &event_loop, ErrorHandler &_error_handler) noexcept;
	~Client() noexcept;

	Client(const Client &) = delete;
	Client &operator=(const Client &) = delete;

	EventLoop &GetEventLoop() const noexcept {
		return poll.GetEventLoop();
	}

	void Close() noexcept;

	AvahiClient *GetClient() noexcept {
		return client;
	}

	void AddListener(ConnectionListener &listener) noexcept {
		listeners.push_front(&listener);
	}

	void RemoveListener(ConnectionListener &listener) noexcept {
		listeners.remove(&listener);
	}

private:
	void ClientCallback(AvahiClient *c, AvahiClientState state) noexcept;
	static void ClientCallback(AvahiClient *c, AvahiClientState state,
				   void *userdata) noexcept;

	void OnReconnectTimer() noexcept;
};

} // namespace Avahi
