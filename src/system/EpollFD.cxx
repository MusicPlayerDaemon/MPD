// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "EpollFD.hxx"
#include "Error.hxx"

EpollFD::EpollFD()
	:fd(AdoptTag{}, ::epoll_create1(EPOLL_CLOEXEC))
{
	if (!fd.IsDefined())
		throw MakeErrno("epoll_create1() failed");
}
