// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <chrono>

namespace Event {

/**
 * The clock used by classes #EventLoop, #CoarseTimerEvent and #FineTimerEvent.
 */
using Clock = std::chrono::steady_clock;

using Duration = Clock::duration;
using TimePoint = Clock::time_point;

} // namespace Event
