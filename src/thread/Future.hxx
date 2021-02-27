/*
 * Copyright 2020-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
