// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
