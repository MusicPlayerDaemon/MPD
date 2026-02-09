// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "event/config.h"

#ifdef _WIN32

#include "WinSelectBackend.hxx"
using EventPollBackend = WinSelectBackend;

#elif defined(USE_EPOLL)

#include "EpollBackend.hxx"
using EventPollBackend = EpollBackend;

#else

#include "PollBackend.hxx"
using EventPollBackend = PollBackend;

#endif
