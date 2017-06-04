/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

class FileOutputStream final : public OutputStream {
	const AllocatedPath path;

#ifdef WIN32
	HANDLE handle = INVALID_HANDLE_VALUE;
#else
	FileDescriptor fd = FileDescriptor::Undefined();
#endif

#ifdef HAVE_LINKAT
	/**
	 * Was O_TMPFILE used?  If yes, then linkat() must be used to
	 * create a link to this file.
	 */
	bool is_tmpfile = false;
#endif

public:
	enum class Mode : uint8_t {
		/**
		 * Create a new file, or replace an existing file.
		 * File contents may not be visible until Commit() has
		 * been called.
		 */
		CREATE,

		/**
		 * Like #CREATE, but no attempt is made to hide file
		 * contents during the transaction (e.g. via O_TMPFILE
		 * or a hidden temporary file).
		 */
		CREATE_VISIBLE,

		/**
		 * Append to a file that already exists.  If it does
		 * not, an exception is thrown.
		 */
		APPEND_EXISTING,

		/**
		 * Like #APPEND_EXISTING, but create the file if it
		 * does not exist.
		 */
		APPEND_OR_CREATE,
	};

private:
	Mode mode;

public:
	explicit FileOutputStream(Path _path, Mode _mode=Mode::CREATE);

	~FileOutputStream() {
		if (IsDefined())
			Cancel();
	}

public:
	Path GetPath() const {
		return path;
	}

	gcc_pure
	uint64_t Tell() const noexcept;

	/* virtual methods from class OutputStream */
	void Write(const void *data, size_t size) override;

	void Commit();
	void Cancel();

private:
	void OpenCreate(bool visible);
	void OpenAppend(bool create);

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
};

#endif
