// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <sys/poll.h>

struct PollEvents {
	static constexpr unsigned READ = POLLIN;
	static constexpr unsigned WRITE = POLLOUT;
	static constexpr unsigned ERROR = POLLERR;
	static constexpr unsigned HANGUP = POLLHUP;
};
