// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Client.hxx"
#include "ConnectionListener.hxx"
#include "ErrorHandler.hxx"
#include "Error.hxx"

#include <avahi-common/error.h>

#include <cassert>

namespace Avahi {

Client::Client(EventLoop &event_loop, ErrorHandler &_error_handler) noexcept
	:error_handler(_error_handler),
	 reconnect_timer(event_loop, BIND_THIS_METHOD(OnReconnectTimer)),
	 poll(event_loop)
{
	reconnect_timer.Schedule({});
}

Client::~Client() noexcept
{
	Close();
}

void
Client::Close() noexcept
{
	if (client != nullptr) {
		for (auto *l : listeners)
			l->OnAvahiDisconnect();

		avahi_client_free(client);
		client = nullptr;
	}

	reconnect_timer.Cancel();
}

void
Client::ClientCallback(AvahiClient *c, AvahiClientState state) noexcept
{
	switch (state) {
	case AVAHI_CLIENT_S_RUNNING:
		for (auto *l : listeners)
			l->OnAvahiConnect(c);

		break;

	case AVAHI_CLIENT_FAILURE:
		if (int error = avahi_client_errno(c);
		    error == AVAHI_ERR_DISCONNECTED) {
			Close();

			reconnect_timer.Schedule(std::chrono::seconds(10));
		} else {
			Close();

			if (!error_handler.OnAvahiError(std::make_exception_ptr(MakeError(error,
											  "Avahi connection error"))))
				return;

			reconnect_timer.Schedule(std::chrono::minutes(1));
		}

		for (auto *l : listeners)
			l->OnAvahiDisconnect();

		break;

	case AVAHI_CLIENT_S_COLLISION:
	case AVAHI_CLIENT_S_REGISTERING:
		for (auto *l : listeners)
			l->OnAvahiChanged();

		break;

	case AVAHI_CLIENT_CONNECTING:
		break;
	}
}

void
Client::ClientCallback(AvahiClient *c, AvahiClientState state,
		       void *userdata) noexcept
{
	auto &client = *(Client *)userdata;
	client.ClientCallback(c, state);
}

void
Client::OnReconnectTimer() noexcept
{
	assert(client == nullptr);

	int error;
	client = avahi_client_new(&poll, AVAHI_CLIENT_NO_FAIL,
				  ClientCallback, this,
				  &error);
	if (client == nullptr) {
		if (!error_handler.OnAvahiError(std::make_exception_ptr(MakeError(error,
										  "Failed to create Avahi client"))))
			return;

		reconnect_timer.Schedule(std::chrono::minutes(1));
		return;
	}
}

} // namespace Avahi
