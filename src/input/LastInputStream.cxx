// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LastInputStream.hxx"
#include "InputStream.hxx"

#include <cassert>

LastInputStream::LastInputStream(EventLoop &event_loop) noexcept
	:close_timer(event_loop, BIND_THIS_METHOD(OnCloseTimer))
{
}

LastInputStream::~LastInputStream() noexcept = default;

void
LastInputStream::Close() noexcept
{
	uri.clear();
	is.reset();
	close_timer.Cancel();
}

void
LastInputStream::OnCloseTimer() noexcept
{
	assert(is);

	uri.clear();
	is.reset();
}
