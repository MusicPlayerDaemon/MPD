/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#ifndef MPD_FILE_READER_HXX
#define MPD_FILE_READER_HXX

#include "check.h"
#include "Reader.hxx"
#include "fs/AllocatedPath.hxx"
#include "Compiler.h"

#ifndef WIN32
#include "system/FileDescriptor.hxx"
#endif

#include <assert.h>

#ifdef WIN32
#include <windows.h>
#endif

class Path;
class FileInfo;

class FileReader final : public Reader {
	AllocatedPath path;

#ifdef WIN32
	HANDLE handle;
#else
	FileDescriptor fd;
#endif

public:
	FileReader(Path _path);

#ifdef WIN32
	FileReader(FileReader &&other)
		:path(std::move(other.path)),
		 handle(other.handle) {
		other.handle = INVALID_HANDLE_VALUE;
	}
#else
	FileReader(FileReader &&other)
		:path(std::move(other.path)),
		 fd(other.fd) {
		other.fd.SetUndefined();
	}
#endif

	~FileReader() {
		if (IsDefined())
			Close();
	}


protected:
	bool IsDefined() const {
#ifdef WIN32
		return handle != INVALID_HANDLE_VALUE;
#else
		return fd.IsDefined();
#endif
	}

public:
#ifndef WIN32
	FileDescriptor GetFD() const {
		return fd;
	}
#endif

	void Close();

	gcc_pure
	FileInfo GetFileInfo() const;

	void Seek(off_t offset);

	/* virtual methods from class Reader */
	size_t Read(void *data, size_t size) override;
};

#endif
