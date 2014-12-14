/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "Blocking.hxx"
#include "Connection.hxx"
#include "Domain.hxx"
#include "event/Call.hxx"
#include "util/Error.hxx"

bool
BlockingNfsOperation::Run(Error &_error)
{
	/* subscribe to the connection, which will invoke either
	   OnNfsConnectionReady() or OnNfsConnectionFailed() */
	BlockingCall(connection.GetEventLoop(),
		    [this](){ connection.AddLease(*this); });

	/* wait for completion */
	if (!LockWaitFinished()) {
		_error.Set(nfs_domain, 0, "Timeout");
		return false;
	}

	/* check for error */
	if (error.IsDefined()) {
		_error = std::move(error);
		return false;
	}

	return true;
}

void
BlockingNfsOperation::OnNfsConnectionReady()
{
	if (!Start(error)) {
		connection.RemoveLease(*this);
		LockSetFinished();
	}
}

void
BlockingNfsOperation::OnNfsConnectionFailed(const Error &_error)
{
	error.Set(_error);
	LockSetFinished();
}

void
BlockingNfsOperation::OnNfsConnectionDisconnected(const Error &_error)
{
	error.Set(_error);
	LockSetFinished();
}

void
BlockingNfsOperation::OnNfsCallback(unsigned status, void *data)
{
	connection.RemoveLease(*this);

	HandleResult(status, data);
	LockSetFinished();
}

void
BlockingNfsOperation::OnNfsError(Error &&_error)
{
	connection.RemoveLease(*this);

	error = std::move(_error);
	LockSetFinished();
}
