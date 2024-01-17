// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

	bool connected = false;

public:
	Client(EventLoop &event_loop, ErrorHandler &_error_handler) noexcept;
	~Client() noexcept;

	Client(const Client &) = delete;
	Client &operator=(const Client &) = delete;

	EventLoop &GetEventLoop() const noexcept {
		return poll.GetEventLoop();
	}

	void Close() noexcept;

	bool IsConnected() const noexcept {
		return connected;
	}

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
