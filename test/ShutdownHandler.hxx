// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TEST_SHUTDOWN_HANDLER_HXX
#define MPD_TEST_SHUTDOWN_HANDLER_HXX

class EventLoop;

class ShutdownHandler {
public:
	explicit ShutdownHandler(EventLoop &loop);
	~ShutdownHandler();
};

#ifdef _WIN32
inline ShutdownHandler::ShutdownHandler(EventLoop &) {}
inline ShutdownHandler::~ShutdownHandler() {}
#endif

#endif
