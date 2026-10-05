// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Blocking.hxx"
#include "Connection.hxx"
#include "event/Call.hxx"
#include "net/TimeoutError.hxx"

void
BlockingNfsOperation::Run()
{
	/* subscribe to the connection, which will invoke either
	   OnNfsConnectionReady() or OnNfsConnectionFailed() */
	BlockingCall(connection.GetEventLoop(),
		    [this](){ connection.AddLease(*this); });

	/* wait for completion */
	if (!LockWaitFinished()) {
		BlockingCall(connection.GetEventLoop(), [this](){
			if (finished)
				/* completed just after the timeout */
				return;

			if (started)
				connection.Cancel(*this, nullptr, {});

			connection.RemoveLease(*this);
		});

		if (!finished)
			// still not finished
			throw TimeoutError{};

		// completed meanwhile: continue below
	}

	/* check for error */
	if (error)
		std::rethrow_exception(std::move(error));
}

void
BlockingNfsOperation::OnNfsConnectionReady() noexcept
{
	try {
		Start();
		started = true;
	} catch (...) {
		error = std::current_exception();
		connection.RemoveLease(*this);
		LockSetFinished();
	}
}

void
BlockingNfsOperation::OnNfsConnectionFailed(std::exception_ptr e) noexcept
{
	error = std::move(e);
	LockSetFinished();
}

void
BlockingNfsOperation::OnNfsConnectionDisconnected(std::exception_ptr e) noexcept
{
	error = std::move(e);
	LockSetFinished();
}

void
BlockingNfsOperation::OnNfsCallback(unsigned status, void *data) noexcept
{
	started = false;
	connection.RemoveLease(*this);

	HandleResult(status, data);
	LockSetFinished();
}

void
BlockingNfsOperation::OnNfsError(std::exception_ptr &&e) noexcept
{
	started = false;
	connection.RemoveLease(*this);

	error = std::move(e);
	LockSetFinished();
}
