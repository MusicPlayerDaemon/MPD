/*
 * Copyright 2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef ODBUS_FILTER_HELPER_HXX
#define ODBUS_FILTER_HELPER_HXX

#include "util/BindMethod.hxx"

#include <dbus/dbus.h>

#include <cassert>

namespace ODBus {

/**
 * A helper for dbus_connection_add_filter() and
 * dbus_connection_remove_filter().
 */
class FilterHelper final {
	DBusConnection *connection = nullptr;

	using Callback = BoundMethod<DBusHandlerResult(DBusConnection *dbus_connection,
						       DBusMessage *message) noexcept>;
	Callback callback;

public:
	FilterHelper() = default;

	FilterHelper(DBusConnection *_connection, Callback _callback) noexcept {
		Add(_connection, _callback);
	}

	~FilterHelper() noexcept {
		Remove();
	}

	operator bool() const noexcept {
		return connection != nullptr;
	}

	DBusConnection *GetConnection() noexcept {
		assert(connection != nullptr);

		return connection;
	}

	void Add(DBusConnection *_connection, Callback _callback) noexcept {
		assert(connection == nullptr);

		connection = _connection;
		callback = _callback;

		dbus_connection_add_filter(connection, HandleMessage, this,
					   nullptr);
	}

	void Remove() noexcept {
		if (!*this)
			return;

		dbus_connection_remove_filter(connection, HandleMessage, this);
		connection = nullptr;
	}

private:
	DBusHandlerResult HandleMessage(DBusConnection *dbus_connection,
					DBusMessage *message) noexcept;

	static DBusHandlerResult HandleMessage(DBusConnection *,
					       DBusMessage *message,
					       void *user_data) noexcept;
};

} // namespace ODBus

#endif
