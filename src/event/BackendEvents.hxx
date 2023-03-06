// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef EVENT_BACKEND_EVENTS_HXX
#define EVENT_BACKEND_EVENTS_HXX

#include "event/Features.h"

#ifdef _WIN32

#include "WinSelectEvents.hxx"
using EventPollBackendEvents = WinSelectEvents;

#elif defined(USE_EPOLL)

#include "EpollEvents.hxx"
using EventPollBackendEvents = EpollEvents;

#else

#include "PollEvents.hxx"
using EventPollBackendEvents = PollEvents;

#endif

#endif
