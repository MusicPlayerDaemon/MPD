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

#ifndef ODBUS_MESSAGE_HXX
#define ODBUS_MESSAGE_HXX

#include <dbus/dbus.h>

#include <utility>

namespace ODBus {

class Message {
	DBusMessage *msg = nullptr;

	explicit Message(DBusMessage *_msg) noexcept
		:msg(_msg) {}

public:
	Message() noexcept = default;

	Message(Message &&src) noexcept
		:msg(std::exchange(src.msg, nullptr)) {}

	~Message() noexcept {
		if (msg != nullptr)
			dbus_message_unref(msg);
	}

	DBusMessage *Get() noexcept {
		return msg;
	}

	Message &operator=(Message &&src) noexcept {
		std::swap(msg, src.msg);
		return *this;
	}

	static Message NewMethodCall(const char *destination,
				     const char *path,
				     const char *iface,
				     const char *method);

	static Message StealReply(DBusPendingCall &pending);

	static Message Pop(DBusConnection &connection) noexcept;

	bool IsDefined() const noexcept {
		return msg != nullptr;
	}

	int GetType() noexcept {
		return dbus_message_get_type(msg);
	}

	const char *GetPath() noexcept {
		return dbus_message_get_path(msg);
	}

	bool HasPath(const char *object_path) noexcept {
		return dbus_message_has_path(msg, object_path);
	}

	const char *GetInterface() noexcept {
		return dbus_message_get_interface(msg);
	}

	bool HasInterface(const char *iface) noexcept {
		return dbus_message_has_interface(msg, iface);
	}

	const char *GetMember() noexcept {
		return dbus_message_get_member(msg);
	}

	bool HasMember(const char *member) noexcept {
		return dbus_message_has_member(msg, member);
	}

	bool IsError(const char *error_name) const noexcept {
		return dbus_message_is_error(msg, error_name);
	}

	const char *GetErrorName() const noexcept {
		return dbus_message_get_error_name(msg);
	}

	const char *GetDestination() const noexcept {
		return dbus_message_get_destination(msg);
	}

	const char *GetSender() const noexcept {
		return dbus_message_get_sender(msg);
	}

	const char *GetSignature() const noexcept {
		return dbus_message_get_signature(msg);
	}

	bool GetNoReply() const noexcept {
		return dbus_message_get_no_reply(msg);
	}

	bool IsMethodCall(const char *iface,
			  const char *method) const noexcept {
		return dbus_message_is_method_call(msg, iface, method);
	}

	bool IsSignal(const char *iface,
		      const char *signal_name) const noexcept {
		return dbus_message_is_signal(msg, iface, signal_name);
	}

	void CheckThrowError();

	template<typename... Args>
	bool GetArgs(DBusError &error, Args... args) noexcept {
		return dbus_message_get_args(msg, &error,
					     std::forward<Args>(args)...,
					     DBUS_TYPE_INVALID);
	}
};

} /* namespace ODBus */

#endif
