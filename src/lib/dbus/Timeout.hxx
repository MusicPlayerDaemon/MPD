// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <dbus/dbus.h>

#include <chrono>

namespace ODBus {

using Timeout = std::chrono::duration<int, std::chrono::milliseconds::period>;

static constexpr Timeout DEFAULT_TIMEOUT{DBUS_TIMEOUT_USE_DEFAULT};
static constexpr Timeout NO_TIMEOUT{DBUS_TIMEOUT_INFINITE};

} /* namespace ODBus */
