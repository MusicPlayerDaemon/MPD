// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "FilterHelper.hxx"

namespace ODBus {

inline DBusHandlerResult
FilterHelper::HandleMessage(DBusConnection *dbus_connection,
			    DBusMessage *message) noexcept
{
	return callback(dbus_connection, message);
}

DBusHandlerResult
FilterHelper::HandleMessage(DBusConnection *dbus_connection,
			    DBusMessage *message,
			    void *user_data) noexcept
{
	auto &fh = *(FilterHelper *)user_data;
	return fh.HandleMessage(dbus_connection, message);
}

} // namespace ODBus
