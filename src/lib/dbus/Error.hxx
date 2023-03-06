// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef ODBUS_ERROR_HXX
#define ODBUS_ERROR_HXX

#include <dbus/dbus.h>

namespace ODBus {

class Error {
	DBusError error;

public:
	Error() noexcept {
		dbus_error_init(&error);
	}

	~Error() noexcept {
		dbus_error_free(&error);
	}

	Error(const Error &) = delete;
	Error &operator=(const Error &) = delete;

	[[gnu::pure]]
	operator bool() const noexcept {
		return dbus_error_is_set(&error);
	}

	operator DBusError &() noexcept {
		return error;
	}

	operator DBusError *() noexcept {
		return &error;
	}

	const char *GetMessage() const noexcept {
		return error.message;
	}

	[[noreturn]]
	void Throw(const char *prefix) const;
	void CheckThrow(const char *prefix) const;
};

} /* namespace ODBus */

#endif
