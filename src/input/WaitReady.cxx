// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "WaitReady.hxx"
#include "InputStream.hxx"
#include "CondHandler.hxx"

void
WaitReady(InputStream &is, std::unique_lock<Mutex> &lock)
{
	CondInputStreamHandler handler;
	const ScopeExchangeInputStreamHandler h{is, &handler};

	handler.cond.wait(lock, [&is]{
		is.Update();
		return is.IsReady();
	});

	is.Check();
}

void
LockWaitReady(InputStream &is)
{
	std::unique_lock lock{is.mutex};
	WaitReady(is, lock);
}
