// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Message.hxx"
#include "ReadIter.hxx"

#include <stdexcept>

ODBus::Message
ODBus::Message::NewMethodCall(const char *destination,
			      const char *path,
			      const char *iface,
			      const char *method)
{
	auto *msg = dbus_message_new_method_call(destination, path,
						 iface, method);
	if (msg == nullptr)
		throw std::runtime_error("dbus_message_new_method_call() failed");

	return Message(msg);
}

ODBus::Message
ODBus::Message::StealReply(DBusPendingCall &pending)
{
	auto *msg = dbus_pending_call_steal_reply(&pending);
	if (msg == nullptr)
		throw std::runtime_error("dbus_pending_call_steal_reply() failed");

	return Message(msg);
}

ODBus::Message
ODBus::Message::Pop(DBusConnection &connection) noexcept
{
	auto *msg = dbus_connection_pop_message(&connection);
	return Message(msg);
}

void
ODBus::Message::CheckThrowError()
{
	if (GetType() != DBUS_MESSAGE_TYPE_ERROR)
		return;

	ReadMessageIter iter(*msg);

	if (iter.GetArgType() != DBUS_TYPE_STRING)
		throw std::runtime_error("No DBUS_MESSAGE_TYPE_ERROR message");

	throw std::runtime_error(iter.GetString());
}
