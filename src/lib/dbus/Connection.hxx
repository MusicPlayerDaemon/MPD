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
