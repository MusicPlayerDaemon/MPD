// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
