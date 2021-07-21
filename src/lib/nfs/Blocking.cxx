/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "Blocking.hxx"
#include "Connection.hxx"
#include "event/Call.hxx"

void
BlockingNfsOperation::Run()
{
	/* subscribe to the connection, which will invoke either
	   OnNfsConnectionReady() or OnNfsConnectionFailed() */
	BlockingCall(connection.GetEventLoop(),
		    [this](){ connection.AddLease(*this); });

	/* wait for completion */
	if (!LockWaitFinished())
		throw std::runtime_error("Timeout");

	/* check for error */
	if (error)
		std::rethrow_exception(std::move(error));
}

void
BlockingNfsOperation::OnNfsConnectionReady() noexcept
{
	try {
		Start();
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
	connection.RemoveLease(*this);

	HandleResult(status, data);
	LockSetFinished();
}

void
BlockingNfsOperation::OnNfsError(std::exception_ptr &&e) noexcept
{
	connection.RemoveLease(*this);

	error = std::move(e);
	LockSetFinished();
}
