// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <chrono>

namespace ODBus {

using Timeout = std::chrono::duration<int, std::chrono::milliseconds::period>;

static constexpr Timeout NO_TIMEOUT{-1};

} /* namespace ODBus */
