// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "EventFD.hxx"
#include "system/Error.hxx"

#include <cassert>

#include <sys/eventfd.h>

EventFD::EventFD()
{
	if (!fd.CreateEventFD(0))
		throw MakeErrno("eventfd() failed");
}

bool
EventFD::Read() noexcept
{
	assert(fd.IsDefined());

	eventfd_t value;
	return fd.Read(&value, sizeof(value)) == (ssize_t)sizeof(value);
}

void
EventFD::Write() noexcept
{
	assert(fd.IsDefined());

	static constexpr eventfd_t value = 1;
	[[maybe_unused]] ssize_t nbytes =
		fd.Write(&value, sizeof(value));
}
