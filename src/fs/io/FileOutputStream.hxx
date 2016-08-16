/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#ifndef MPD_FILE_OUTPUT_STREAM_HXX
#define MPD_FILE_OUTPUT_STREAM_HXX

#include "check.h"
#include "OutputStream.hxx"
#include "fs/AllocatedPath.hxx"
#include "Compiler.h"

#ifndef WIN32
#include "system/FileDescriptor.hxx"
#endif

#include <assert.h>
#include <stdint.h>

#ifdef WIN32
#include <windows.h>
#endif

class Path;

class BaseFileOutputStream : public OutputStream {
	const AllocatedPath path;

#ifdef WIN32
	HANDLE handle = INVALID_HANDLE_VALUE;
#else
	FileDescriptor fd = FileDescriptor::Undefined();
#endif

protected:
#ifdef WIN32
	template<typename P>
	BaseFileOutputStream(P &&_path)
		:path(std::forward<P>(_path)) {}
#else
	template<typename P>
	BaseFileOutputStream(P &&_path)
		:path(std::forward<P>(_path)) {}
#endif

	~BaseFileOutputStream() {
		assert(!IsDefined());
	}

#ifdef WIN32
	void SetHandle(HANDLE _handle) {
		assert(!IsDefined());

		handle = _handle;

		assert(IsDefined());
	}
#else
	FileDescriptor &SetFD() {
		assert(!IsDefined());

		return fd;
	}

	const FileDescriptor &GetFD() const {
		return fd;
	}
#endif

	bool Close() {
		assert(IsDefined());

#ifdef WIN32
		CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
		return true;
#else
		return fd.Close();
#endif
	}

#ifdef WIN32
	bool SeekEOF() {
		return SetFilePointer(handle, 0, nullptr,
				      FILE_END) != 0xffffffff;
	}
#endif

	bool IsDefined() const {
#ifdef WIN32
		return handle != INVALID_HANDLE_VALUE;
#else
		return fd.IsDefined();
#endif
	}

public:
	Path GetPath() const {
		return path;
	}

	gcc_pure
	uint64_t Tell() const;

	/* virtual methods from class OutputStream */
	void Write(const void *data, size_t size) override;
};

class FileOutputStream final : public BaseFileOutputStream {
#ifdef HAVE_LINKAT
	/**
	 * Was O_TMPFILE used?  If yes, then linkat() must be used to
	 * create a link to this file.
	 */
	bool is_tmpfile;
#endif

public:
	FileOutputStream(Path _path);

	~FileOutputStream() {
		if (IsDefined())
			Cancel();
	}

	void Commit();
	void Cancel();
};

class AppendFileOutputStream final : public BaseFileOutputStream {
public:
	AppendFileOutputStream(Path _path);

	~AppendFileOutputStream() {
		if (IsDefined())
			Close();
	}

	void Commit();
};

#endif
