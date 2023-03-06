// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef ODBUS_CONNECTION_HXX
#define ODBUS_CONNECTION_HXX

#include <dbus/dbus.h>

#include <utility>

namespace ODBus {

/**
 * OO wrapper for a #DBusConnection.
 */
class Connection {
	DBusConnection *c = nullptr;

	explicit Connection(DBusConnection *_c) noexcept
		:c(_c) {}

public:
	Connection() noexcept = default;

	Connection(const Connection &src) noexcept
		:c(dbus_connection_ref(src.c)) {}

	Connection(Connection &&src) noexcept
		:c(std::exchange(src.c, nullptr)) {}

	~Connection() noexcept {
		if (c != nullptr)
			dbus_connection_unref(c);
	}

	Connection &operator=(Connection &&src) noexcept {
		std::swap(c, src.c);
		return *this;
	}

	static Connection GetSystem();
	static Connection GetSystemPrivate();
	static Connection Open(const char *address);

	operator DBusConnection *() noexcept {
		return c;
	}

	operator DBusConnection &() noexcept {
		return *c;
	}

	operator bool() const noexcept {
		return c != nullptr;
	}

	void Close() noexcept {
		dbus_connection_close(c);
	}
};

} /* namespace ODBus */

#endif
