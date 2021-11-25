/*
 * Copyright 2007-2017 Content Management AG
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

#ifndef ODBUS_PENDING_CALL_HXX
#define ODBUS_PENDING_CALL_HXX

#include <dbus/dbus.h>

#include <stdexcept>
#include <utility>

namespace ODBus {

class PendingCall {
	DBusPendingCall *pending = nullptr;

	explicit PendingCall(DBusPendingCall *_pending) noexcept
		:pending(_pending) {}

public:
	PendingCall() noexcept = default;

	PendingCall(PendingCall &&src) noexcept
		:pending(std::exchange(src.pending, nullptr)) {}

	~PendingCall() noexcept {
		if (pending != nullptr)
			dbus_pending_call_unref(pending);
	}

	operator bool() const noexcept {
		return pending;
	}

	DBusPendingCall *Get() noexcept {
		return pending;
	}

	PendingCall &operator=(PendingCall &&src) noexcept {
		std::swap(pending, src.pending);
		return *this;
	}

	static PendingCall SendWithReply(DBusConnection *connection,
					 DBusMessage *message,
					 int timeout_milliseconds=-1) {
		DBusPendingCall *pending;
		if (!dbus_connection_send_with_reply(connection,
						     message,
						     &pending,
						     timeout_milliseconds))
			throw std::runtime_error("dbus_connection_send_with_reply() failed");

		if (pending == nullptr)
			throw std::runtime_error("dbus_connection_send_with_reply() failed with pending=NULL");

		return PendingCall(pending);
	}

	bool SetNotify(DBusPendingCallNotifyFunction function,
		       void *user_data,
		       DBusFreeFunction free_user_data=nullptr) noexcept {
		return dbus_pending_call_set_notify(pending,
						    function, user_data,
						    free_user_data);
	}

	void Cancel() noexcept {
		dbus_pending_call_cancel(pending);
	}

	void Block() noexcept {
		dbus_pending_call_block(pending);
	}
};

} /* namespace ODBus */

#endif
