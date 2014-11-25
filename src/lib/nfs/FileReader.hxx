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

#ifndef MPD_NFS_FILE_READER_HXX
#define MPD_NFS_FILE_READER_HXX

#include "check.h"
#include "Lease.hxx"
#include "Callback.hxx"
#include "event/DeferredMonitor.hxx"
#include "Compiler.h"

#include <string>

#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>

struct nfsfh;
class NfsConnection;

class NfsFileReader : NfsLease, NfsCallback, DeferredMonitor {
	enum class State {
		INITIAL,
		DEFER,
		MOUNT,
		OPEN,
		STAT,
		READ,
		IDLE,
	};

	State state;

	std::string server, export_name;
	const char *path;

	NfsConnection *connection;

	nfsfh *fh;

public:
	NfsFileReader();
	~NfsFileReader();

	void Close();
	void DeferClose();

	bool Open(const char *uri, Error &error);
	bool Read(uint64_t offset, size_t size, Error &error);
	void CancelRead();

	bool IsIdle() const {
		return state == State::IDLE;
	}

protected:
	virtual void OnNfsFileOpen(uint64_t size) = 0;
	virtual void OnNfsFileRead(const void *data, size_t size) = 0;
	virtual void OnNfsFileError(Error &&error) = 0;

private:
	/**
	 * Cancel the current operation, if any.  The NfsLease must be
	 * unregistered already.
	 */
	void CancelOrClose();

	void OpenCallback(nfsfh *_fh);
	void StatCallback(const struct stat *st);

	/* virtual methods from NfsLease */
	void OnNfsConnectionReady() final;
	void OnNfsConnectionFailed(const Error &error) final;
	void OnNfsConnectionDisconnected(const Error &error) final;

	/* virtual methods from NfsCallback */
	void OnNfsCallback(unsigned status, void *data) final;
	void OnNfsError(Error &&error) final;

	/* virtual methods from DeferredMonitor */
	void RunDeferred() final;
};

#endif
