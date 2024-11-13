// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SignalFD.hxx"
#include "Error.hxx"
#include "util/SpanCast.hxx"

#include <cassert>

#include <sys/signalfd.h>

void
SignalFD::Create(const sigset_t &mask)
{
	int new_fd = ::signalfd(fd.Get(), &mask, SFD_NONBLOCK|SFD_CLOEXEC);
	if (new_fd < 0)
		throw MakeErrno("signalfd() failed");

	if (!fd.IsDefined()) {
		fd = UniqueFileDescriptor{new_fd};
	}

	assert(new_fd == fd.Get());
}

int
SignalFD::Read() noexcept
{
	assert(fd.IsDefined());

	signalfd_siginfo info;
	return fd.Read(ReferenceAsWritableBytes(info)) > 0
		? info.ssi_signo
		: -1;
}
