// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include "event/config.h" // for USE_EPOLL

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
