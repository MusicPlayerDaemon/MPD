// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SignalFD.hxx"
#include "Error.hxx"

#include <cassert>

#include <sys/signalfd.h>

void
SignalFD::Create(const sigset_t &mask)
{
	if (!fd.CreateSignalFD(&mask))
		throw MakeErrno("signalfd() failed");
}

int
SignalFD::Read() noexcept
{
	assert(fd.IsDefined());

	signalfd_siginfo info;
	return fd.Read(&info, sizeof(info)) > 0
		? info.ssi_signo
		: -1;
}
