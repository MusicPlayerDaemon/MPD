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

#include "config.h" /* must be first for large file support */
#include "FileInputPlugin.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "fs/Path.hxx"
#include "fs/FileInfo.hxx"
#include "fs/io/FileReader.hxx"
#include "system/FileDescriptor.hxx"
#include "util/RuntimeError.hxx"

#include <sys/stat.h>
#include <fcntl.h>

class FileInputStream final : public InputStream {
	FileReader reader;

public:
	FileInputStream(const char *path, FileReader &&_reader, off_t _size,
			Mutex &_mutex, Cond &_cond)
		:InputStream(path, _mutex, _cond),
		 reader(std::move(_reader)) {
		size = _size;
		seekable = true;
		SetReady();
	}

	/* virtual methods from InputStream */

	bool IsEOF() noexcept override {
		return GetOffset() >= GetSize();
	}

	size_t Read(void *ptr, size_t size) override;
	void Seek(offset_type offset) override;
};

InputStreamPtr
OpenFileInputStream(Path path,
		    Mutex &mutex, Cond &cond)
{
	FileReader reader(path);

	const FileInfo info = reader.GetFileInfo();

	if (!info.IsRegular())
		throw FormatRuntimeError("Not a regular file: %s",
					 path.c_str());

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(reader.GetFD().Get(), (off_t)0, info.GetSize(),
		      POSIX_FADV_SEQUENTIAL);
#endif

	return std::make_unique<FileInputStream>(path.ToUTF8().c_str(),
						 std::move(reader), info.GetSize(),
						 mutex, cond);
}

static InputStreamPtr
input_file_open(gcc_unused const char *filename,
		gcc_unused Mutex &mutex, gcc_unused Cond &cond)
{
	/* dummy method; use OpenFileInputStream() instead */

	return nullptr;
}

void
FileInputStream::Seek(offset_type new_offset)
{
	{
		const ScopeUnlock unlock(mutex);
		reader.Seek((off_t)new_offset);
	}

	offset = new_offset;
}

size_t
FileInputStream::Read(void *ptr, size_t read_size)
{
	size_t nbytes;

	{
		const ScopeUnlock unlock(mutex);
		nbytes = reader.Read(ptr, read_size);
	}

	offset += nbytes;
	return nbytes;
}

const InputPlugin input_plugin_file = {
	"file",
	nullptr,
	nullptr,
	input_file_open,
};
