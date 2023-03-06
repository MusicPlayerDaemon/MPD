// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef THREAD_COND_HXX
#define THREAD_COND_HXX

#ifdef _WIN32

#include "WindowsCond.hxx"
class Cond : public WindowsCond {};

#else

#include <condition_variable>
using Cond = std::condition_variable;

#endif

#endif
