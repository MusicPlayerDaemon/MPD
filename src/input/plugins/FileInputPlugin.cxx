// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FileInputPlugin.hxx"
#include "../InputStream.hxx"
#include "fs/Path.hxx"
#include "fs/FileInfo.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "io/FileReader.hxx"
#include "io/FileDescriptor.hxx"

#include <sys/stat.h>
#include <fcntl.h>

class FileInputStream final : public InputStream {
	FileReader reader;

public:
	FileInputStream(const char *path, FileReader &&_reader, off_t _size,
			Mutex &_mutex)
		:InputStream(path, _mutex),
		 reader(std::move(_reader)) {
		size = _size;
		seekable = true;
		SetReady();
	}

	/* virtual methods from InputStream */

	[[nodiscard]] bool IsEOF() const noexcept override {
		return GetOffset() >= GetSize();
	}

	size_t Read(std::unique_lock<Mutex> &lock,
		    void *ptr, size_t size) override;
	void Seek(std::unique_lock<Mutex> &lock,
		  offset_type offset) override;
};

InputStreamPtr
OpenFileInputStream(Path path, Mutex &mutex)
{
	FileReader reader(path);

	const FileInfo info = reader.GetFileInfo();

	if (!info.IsRegular())
		throw FmtRuntimeError("Not a regular file: {}", path);

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(reader.GetFD().Get(), (off_t)0, info.GetSize(),
		      POSIX_FADV_SEQUENTIAL);
#endif

	return std::make_unique<FileInputStream>(path.ToUTF8Throw().c_str(),
						 std::move(reader), info.GetSize(),
						 mutex);
}

void
FileInputStream::Seek(std::unique_lock<Mutex> &,
		      offset_type new_offset)
{
	{
		const ScopeUnlock unlock(mutex);
		reader.Seek((off_t)new_offset);
	}

	offset = new_offset;
}

size_t
FileInputStream::Read(std::unique_lock<Mutex> &,
		      void *ptr, size_t read_size)
{
	size_t nbytes;

	{
		const ScopeUnlock unlock(mutex);
		nbytes = reader.Read({static_cast<std::byte *>(ptr), read_size});
	}

	if (nbytes == 0 && !IsEOF())
		throw FmtRuntimeError("Unexpected end of file {}"
				      " at {} of {}",
				      GetURI(), GetOffset(), GetSize());

	offset += nbytes;
	return nbytes;
}
