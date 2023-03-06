// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_URING_HXX
#define MPD_INPUT_URING_HXX

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

class EventLoop;

void
InitUringInputPlugin(EventLoop &event_loop) noexcept;

InputStreamPtr
OpenUringInputStream(const char *path, Mutex &mutex);

#endif
