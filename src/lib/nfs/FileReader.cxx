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
#include "FileReader.hxx"
#include "Glue.hxx"
#include "Base.hxx"
#include "Connection.hxx"
#include "Domain.hxx"
#include "event/Call.hxx"
#include "IOThread.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"

#include <utility>

#include <assert.h>
#include <string.h>
#include <fcntl.h>

NfsFileReader::NfsFileReader()
	:DeferredMonitor(io_thread_get()), state(State::INITIAL)
{
}

NfsFileReader::~NfsFileReader()
{
	assert(state == State::INITIAL);
}

void
NfsFileReader::Close()
{
	if (state == State::INITIAL)
		return;

	if (state == State::DEFER) {
		state = State::INITIAL;
		DeferredMonitor::Cancel();
		return;
	}

	/* this cancels State::MOUNT */
	connection->RemoveLease(*this);

	CancelOrClose();
}

void
NfsFileReader::CancelOrClose()
{
	assert(state != State::INITIAL &&
	       state != State::DEFER);

	if (state == State::IDLE)
		/* no async operation in progress: can close
		   immediately */
		connection->Close(fh);
	else if (state > State::OPEN)
		/* one async operation in progress: cancel it and
		   defer the nfs_close_async() call */
		connection->CancelAndClose(fh, *this);
	else if (state > State::MOUNT)
		/* we don't have a file handle yet - just cancel the
		   async operation */
		connection->Cancel(*this);

	state = State::INITIAL;
}

void
NfsFileReader::DeferClose()
{
	BlockingCall(io_thread_get(), [this](){ Close(); });
}

bool
NfsFileReader::Open(const char *uri, Error &error)
{
	assert(state == State::INITIAL);

	if (!StringStartsWith(uri, "nfs://")) {
		error.Set(nfs_domain, "Malformed nfs:// URI");
		return false;
	}

	uri += 6;

	const char *slash = strchr(uri, '/');
	if (slash == nullptr) {
		error.Set(nfs_domain, "Malformed nfs:// URI");
		return false;
	}

	server = std::string(uri, slash);

	uri = slash;

	const char *new_path = nfs_check_base(server.c_str(), uri);
	if (new_path != nullptr) {
		export_name = std::string(uri, new_path);
		if (*new_path == 0)
			new_path = "/";
		path = new_path;
	} else {
		slash = strrchr(uri + 1, '/');
		if (slash == nullptr || slash[1] == 0) {
			error.Set(nfs_domain, "Malformed nfs:// URI");
			return false;
		}

		export_name = std::string(uri, slash);
		path = slash;
	}

	state = State::DEFER;
	DeferredMonitor::Schedule();
	return true;
}

bool
NfsFileReader::Read(uint64_t offset, size_t size, Error &error)
{
	assert(state == State::IDLE);

	if (!connection->Read(fh, offset, size, *this, error))
		return false;

	state = State::READ;
	return true;
}

void
NfsFileReader::CancelRead()
{
	if (state == State::READ) {
		connection->Cancel(*this);
		state = State::IDLE;
	}
}

void
NfsFileReader::OnNfsConnectionReady()
{
	assert(state == State::MOUNT);

	Error error;
	if (!connection->Open(path, O_RDONLY, *this, error)) {
		OnNfsFileError(std::move(error));
		return;
	}

	state = State::OPEN;
}

void
NfsFileReader::OnNfsConnectionFailed(const Error &error)
{
	assert(state == State::MOUNT);

	state = State::INITIAL;

	Error copy;
	copy.Set(error);
	OnNfsFileError(std::move(copy));
}

void
NfsFileReader::OnNfsConnectionDisconnected(const Error &error)
{
	assert(state > State::MOUNT);

	CancelOrClose();

	Error copy;
	copy.Set(error);
	OnNfsFileError(std::move(copy));
}

inline void
NfsFileReader::OpenCallback(nfsfh *_fh)
{
	assert(state == State::OPEN);
	assert(connection != nullptr);
	assert(_fh != nullptr);

	fh = _fh;

	Error error;
	if (!connection->Stat(fh, *this, error)) {
		OnNfsFileError(std::move(error));
		return;
	}

	state = State::STAT;
}

inline void
NfsFileReader::StatCallback(const struct stat *st)
{
	assert(state == State::STAT);
	assert(connection != nullptr);
	assert(fh != nullptr);
	assert(st != nullptr);

	if (!S_ISREG(st->st_mode)) {
		OnNfsFileError(Error(nfs_domain, "Not a regular file"));
		return;
	}

	state = State::IDLE;

	OnNfsFileOpen(st->st_size);
}

void
NfsFileReader::OnNfsCallback(unsigned status, void *data)
{
	switch (state) {
	case State::INITIAL:
	case State::DEFER:
	case State::MOUNT:
	case State::IDLE:
		assert(false);
		gcc_unreachable();

	case State::OPEN:
		OpenCallback((struct nfsfh *)data);
		break;

	case State::STAT:
		StatCallback((const struct stat *)data);
		break;

	case State::READ:
		state = State::IDLE;
		OnNfsFileRead(data, status);
		break;
	}
}

void
NfsFileReader::OnNfsError(Error &&error)
{
	switch (state) {
	case State::INITIAL:
	case State::DEFER:
	case State::MOUNT:
	case State::IDLE:
		assert(false);
		gcc_unreachable();

	case State::OPEN:
		connection->RemoveLease(*this);
		state = State::INITIAL;
		break;

	case State::STAT:
		connection->RemoveLease(*this);
		connection->Close(fh);
		state = State::INITIAL;
		break;

	case State::READ:
		state = State::IDLE;
		break;
	}

	OnNfsFileError(std::move(error));
}

void
NfsFileReader::RunDeferred()
{
	assert(state == State::DEFER);

	state = State::MOUNT;

	connection = &nfs_get_connection(server.c_str(), export_name.c_str());
	connection->AddLease(*this);
}
