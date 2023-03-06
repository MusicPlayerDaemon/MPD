// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef THREAD_UTIL_HXX
#define THREAD_UTIL_HXX

/**
 * Lower the current thread's priority to "idle" (very low).
 */
void
SetThreadIdlePriority() noexcept;

/**
 * Raise the current thread's priority to "real-time" (very high).
 *
 * Throws std::system_error on error.
 */
void
SetThreadRealtime();

#endif
