// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <sys/epoll.h>

struct EpollEvents {
	static constexpr unsigned READ = EPOLLIN;
	static constexpr unsigned WRITE = EPOLLOUT;
	static constexpr unsigned ERROR = EPOLLERR;
	static constexpr unsigned HANGUP = EPOLLHUP;
};
