/*
 * Copyright (C) 2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef ODBUS_ASYNC_REQUEST_HXX
#define ODBUS_ASYNC_REQUEST_HXX

#include "PendingCall.hxx"
#include "Message.hxx"

#include <cassert>
#include <functional>

namespace ODBus {

/**
 * Helper class which makes sending messages and receiving the
 * response asynchronously easy.
 *
 * Remember to always cancel pending operations before destructing
 * this class.
 */
class AsyncRequest {
	PendingCall pending_call;

	std::function<void(Message)> callback;

public:
	operator bool() const noexcept {
		return pending_call;
	}

	/**
	 * Send a message on the specified connection and invoke the
	 * given callback upon completion (or error).
	 *
	 * The callback should invoke Message::CheckThrowError() to
	 * check for errors.
	 *
	 * This object must be kept around until the operation
	 * completes.  It can only be reused after completion.
	 */
	template<typename F>
	void Send(DBusConnection &connection,
		  DBusMessage &message,
		  int timeout_milliseconds,
		  F &&_callback) {
		assert(!pending_call);

		callback = std::forward<F>(_callback);
		pending_call = PendingCall::SendWithReply(&connection,
							  &message,
							  timeout_milliseconds);
		pending_call.SetNotify(Notify, this);
	}

	template<typename F>
	void Send(DBusConnection &connection,
		  DBusMessage &message,
		  F &&_callback) {
		Send(connection, message, -1, std::forward<F>(_callback));
	}

	void Cancel() {
		pending_call.Cancel();
	}

private:
	void Notify(DBusPendingCall *pending) noexcept {
		assert(pending_call.Get() == pending);
		pending_call = {};
		std::exchange(callback, {})(Message::StealReply(*pending));
	}

	static void Notify(DBusPendingCall *pending,
			   void *user_data) noexcept {
		auto &ar = *(AsyncRequest *)user_data;
		ar.Notify(pending);
	}
};

} /* namespace ODBus */

#endif
