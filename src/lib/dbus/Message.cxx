/*
 * Copyright 2007-2019 Content Management AG
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
