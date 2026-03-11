// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

/**
 * Attempt to determine the number of threads of the current process.
 *
 * @return the number of threads or 0 on error
 */
[[gnu::pure]]
unsigned
ProcStatusThreads() noexcept;
