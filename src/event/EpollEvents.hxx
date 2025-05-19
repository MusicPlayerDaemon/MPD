// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <sys/epoll.h>

struct EpollEvents {
	static constexpr unsigned READ = EPOLLIN;
	static constexpr unsigned EXCEPTIONAL = EPOLLPRI;
	static constexpr unsigned WRITE = EPOLLOUT;
	static constexpr unsigned ERROR = EPOLLERR;
	static constexpr unsigned HANGUP = EPOLLHUP;

	/**
	 * A mask containing all events which indicate a dead socket
	 * connection (i.e. error or hangup).  It may still be
	 * possible to receive pending data from the socket buffer.
	 */
	static constexpr unsigned DEAD_MASK = ERROR|HANGUP;
};
