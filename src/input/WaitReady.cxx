// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "WaitReady.hxx"
#include "InputStream.hxx"
#include "CondHandler.hxx"
#include "net/TimeoutError.hxx"

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

void
WaitReady(InputStream &is, std::unique_lock<Mutex> &lock,
	  std::chrono::steady_clock::duration timeout)
{
	CondInputStreamHandler handler;
	const ScopeExchangeInputStreamHandler h{is, &handler};

	const bool success = handler.cond.wait_for(lock, timeout, [&is]{
		is.Update();
		return is.IsReady();
	});

	if (!success)
		throw TimeoutError{};

	is.Check();
}

void
LockWaitReady(InputStream &is,
	      std::chrono::steady_clock::duration timeout)
{
	std::unique_lock lock{is.mutex};
	WaitReady(is, lock, timeout);
}
