// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "UringManager.hxx"
#include "util/PrintException.hxx"

namespace Uring {

void
Manager::OnSocketReady(unsigned) noexcept
{
	try {
		DispatchCompletions();
	} catch (...) {
		PrintException(std::current_exception());
	}
}

void
Manager::OnIdle() noexcept
{
	try {
		Submit();
	} catch (...) {
		PrintException(std::current_exception());
	}
}

} // namespace Uring
