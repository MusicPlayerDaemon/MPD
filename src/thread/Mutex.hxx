// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <mutex>

#ifdef _WIN32

#include "CriticalSection.hxx"
using Mutex = CriticalSection;
using RecursiveMutex = CriticalSection;

#else

using Mutex = std::mutex;
using RecursiveMutex = std::recursive_mutex;

#endif
