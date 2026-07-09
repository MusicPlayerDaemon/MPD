// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <utility>

struct DBusConnection;

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

	Connection(const Connection &src) noexcept;

	Connection(Connection &&src) noexcept
		:c(std::exchange(src.c, nullptr)) {}

	~Connection() noexcept {
		if (c != nullptr)
			Unref(c);
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

	void Close() noexcept;

private:
	static void Unref(DBusConnection *c) noexcept;
};

} /* namespace ODBus */
