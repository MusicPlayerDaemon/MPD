// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef THREAD_FUTURE_HXX
#define THREAD_FUTURE_HXX

#ifdef _WIN32

#include "WindowsFuture.hxx"

template <typename R>
using Future = WinFuture<R>;
template <typename R>
using Promise = WinPromise<R>;

#else

#include <future>

template <typename R>
using Future = std::future<R>;
template <typename R>
using Promise = std::promise<R>;

#endif

#endif
