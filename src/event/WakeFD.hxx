// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_WAKE_FD_HXX
#define MPD_WAKE_FD_HXX

#include "net/SocketDescriptor.hxx"
#include "event/Features.h"

#ifdef USE_EVENTFD
#include "system/EventFD.hxx"
#else
#include "system/EventPipe.hxx"
#endif

class WakeFD {
#ifdef USE_EVENTFD
	EventFD fd;
#else
	EventPipe fd;
#endif

public:
	SocketDescriptor GetSocket() const noexcept {
#ifdef _WIN32
		return fd.Get();
#else
		return SocketDescriptor::FromFileDescriptor(fd.Get());
#endif
	}

	bool Read() noexcept {
		return fd.Read();
	}

	void Write() noexcept {
		fd.Write();
	}
};

#endif
