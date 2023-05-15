// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "FileOutputStream.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/ToBuffer.hxx"

#ifdef _WIN32
#include <tchar.h>
#endif

#ifdef __linux__
#include <fcntl.h>
#endif

#ifdef __linux__
FileOutputStream::FileOutputStream(FileDescriptor _directory_fd,
				   Path _path, Mode _mode)
	:path(_path),
	 directory_fd(_directory_fd),
	 mode(_mode)
{
	Open();
}
#endif

FileOutputStream::FileOutputStream(Path _path, Mode _mode)
	:path(_path),
#ifdef __linux__
	 directory_fd(AT_FDCWD),
#endif
	 mode(_mode)
{
	Open();
}

inline void
FileOutputStream::Open()
{
	switch (mode) {
	case Mode::CREATE:
		OpenCreate(false);
		break;

	case Mode::CREATE_VISIBLE:
		OpenCreate(true);
		break;

	case Mode::APPEND_EXISTING:
		OpenAppend(false);
		break;

	case Mode::APPEND_OR_CREATE:
		OpenAppend(true);
		break;
	}
}

#ifdef _WIN32

inline void
FileOutputStream::OpenCreate(bool visible)
{
	if (!visible) {
		/* attempt to create a temporary file */
		tmp_path = path.WithSuffix(_T(".tmp"));
		Delete(tmp_path);

		handle = CreateFile(tmp_path.c_str(), GENERIC_WRITE, 0, nullptr,
				    CREATE_NEW,
				    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,
				    nullptr);
		if (handle != INVALID_HANDLE_VALUE)
			return;

	}

	handle = CreateFile(path.c_str(), GENERIC_WRITE, 0, nullptr,
			    CREATE_ALWAYS,
			    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,
			    nullptr);
	if (!IsDefined())
		throw FmtLastError("Failed to create {}", path);
}

inline void
FileOutputStream::OpenAppend(bool create)
{
	handle = CreateFile(path.c_str(), GENERIC_WRITE, 0, nullptr,
			    create ? OPEN_ALWAYS : OPEN_EXISTING,
			    FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,
			    nullptr);
	if (!IsDefined())
		throw FmtLastError("Failed to append to {}", path);

	if (!SeekEOF()) {
		auto code = GetLastError();
		Close();
		throw FmtLastError(code, "Failed seek end-of-file of {}", path);
	}

}

uint64_t
FileOutputStream::Tell() const noexcept
{
	LONG high = 0;
	DWORD low = SetFilePointer(handle, 0, &high, FILE_CURRENT);
	if (low == 0xffffffff)
		return 0;

	return uint64_t(high) << 32 | uint64_t(low);
}

void
FileOutputStream::Write(std::span<const std::byte> src)
{
	assert(IsDefined());

	DWORD nbytes;
	if (!WriteFile(handle, src.data(), src.size(), &nbytes, nullptr))
		throw FmtLastError("Failed to write to {}", GetPath());

	if (size_t(nbytes) != src.size())
		throw FmtLastError(DWORD{ERROR_DISK_FULL},
				   "Failed to write to {}",
				   GetPath());
}

void
FileOutputStream::Sync()
{
	assert(IsDefined());

	if (!FlushFileBuffers(handle))
		throw FmtLastError("Failed to sync {}", GetPath());
}

void
FileOutputStream::Commit()
try {
	assert(IsDefined());

	Close();

	if (tmp_path != nullptr)
		RenameOrThrow(tmp_path, path);
} catch (...) {
	if (tmp_path != nullptr)
		Delete(tmp_path);
	throw;
}

#else

#include <cerrno>

#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_O_TMPFILE
#ifndef O_TMPFILE
/* supported since Linux 3.11 */
#define __O_TMPFILE 020000000
#define O_TMPFILE (__O_TMPFILE | O_DIRECTORY)
#include <stdio.h>
#endif

/**
 * Open a file using Linux's O_TMPFILE for writing the given file.
 */
static bool
OpenTempFile(FileDescriptor directory_fd,
	     FileDescriptor &fd, Path path) noexcept
{
	if (directory_fd != FileDescriptor(AT_FDCWD))
		return fd.Open(directory_fd, ".", O_TMPFILE|O_WRONLY, 0666);

	const auto directory = path.GetDirectoryName();
	if (directory.IsNull())
		return false;

	return fd.Open(directory.c_str(), O_TMPFILE|O_WRONLY, 0666);
}

#endif /* HAVE_O_TMPFILE */

inline void
FileOutputStream::OpenCreate([[maybe_unused]] bool visible)
{
#ifdef HAVE_O_TMPFILE
	/* try Linux's O_TMPFILE first */
	if (!visible && OpenTempFile(directory_fd, fd, GetPath())) {
		is_tmpfile = true;
		return;
	}
#endif

	if (!visible) {
		/* attempt to create a temporary file */
		tmp_path = path + ".tmp";
		Delete(tmp_path);

		if (fd.Open(
#ifdef __linux__
			    directory_fd,
#endif
			    tmp_path.c_str(),
			    O_WRONLY|O_CREAT|O_EXCL,
			    0666))
			return;

	}

	/* fall back to plain POSIX */
	if (!fd.Open(
#ifdef __linux__
		    directory_fd,
#endif
		    GetPath().c_str(),
		    O_WRONLY|O_CREAT|O_TRUNC,
		    0666))
		throw FmtErrno("Failed to create {}", GetPath());
}

inline void
FileOutputStream::OpenAppend(bool create)
{
	int flags = O_WRONLY|O_APPEND;
	if (create)
		flags |= O_CREAT;

	if (!fd.Open(
#ifdef __linux__
		     directory_fd,
#endif
		     path.c_str(), flags))
		throw FmtErrno("Failed to append to {}", path);
}

uint64_t
FileOutputStream::Tell() const noexcept
{
	return fd.Tell();
}

void
FileOutputStream::Write(std::span<const std::byte> src)
{
	assert(IsDefined());

	ssize_t nbytes = fd.Write(src.data(), src.size());
	if (nbytes < 0)
		throw FmtErrno("Failed to write to {}", GetPath());
	else if ((size_t)nbytes < src.size())
		throw FmtErrno(ENOSPC, "Failed to write to {}", GetPath());
}

void
FileOutputStream::Sync()
{
	assert(IsDefined());

#ifdef __linux__
	const bool success = fdatasync(fd.Get()) == 0;
#else
	const bool success = fsync(fd.Get()) == 0;
#endif
	if (!success)
		throw FmtErrno("Failed to sync {}", GetPath());
}

void
FileOutputStream::Commit()
try {
	assert(IsDefined());

#ifdef HAVE_O_TMPFILE
	if (is_tmpfile) {
		unlinkat(directory_fd.Get(), GetPath().c_str(), 0);

		/* hard-link the temporary file to the final path */
		if (linkat(AT_FDCWD,
			   FmtBuffer<64>("/proc/self/fd/{}", fd.Get()),
			   directory_fd.Get(), path.c_str(),
			   AT_SYMLINK_FOLLOW) < 0)
			throw FmtErrno("Failed to commit {}", path);
	}
#endif

	if (!Close()) {
		throw FmtErrno("Failed to commit {}", path);
	}

	if (tmp_path != nullptr)
		RenameOrThrow(tmp_path, path);
} catch (...) {
	if (tmp_path != nullptr)
		Delete(tmp_path);
	throw;
}

#endif

void
FileOutputStream::Cancel() noexcept
{
	assert(IsDefined());

	Close();

	if (tmp_path != nullptr) {
		Delete(tmp_path);
		return;
	}

	switch (mode) {
	case Mode::CREATE:
#ifdef HAVE_O_TMPFILE
		if (!is_tmpfile)
#endif
			Delete(GetPath());
		break;

	case Mode::CREATE_VISIBLE:
	case Mode::APPEND_EXISTING:
	case Mode::APPEND_OR_CREATE:
		/* can't roll this back */
		break;
	}
}

inline void
FileOutputStream::RenameOrThrow([[maybe_unused]] Path old_path,
				[[maybe_unused]] Path new_path) const
{
	assert(old_path != nullptr);
	assert(new_path != nullptr);

#ifdef _WIN32
	if (!MoveFileEx(old_path.c_str(), new_path.c_str(),
			MOVEFILE_REPLACE_EXISTING))
		throw MakeLastError("Failed to rename file");
#elif defined(__linux__)
	if (renameat(directory_fd.Get(), tmp_path.c_str(),
		     directory_fd.Get(), path.c_str()) < 0)
		throw MakeErrno("Failed to rename file");
#else
	if (rename(tmp_path.c_str(), path.c_str()))
		throw MakeErrno("Failed to rename file");
#endif
}

inline void
FileOutputStream::Delete(Path delete_path) const noexcept
{
	assert(delete_path != nullptr);

#ifdef _WIN32
	DeleteFile(delete_path.c_str());
#elif defined(__linux__)
	unlinkat(directory_fd.Get(), delete_path.c_str(), 0);
#else
	unlink(delete_path.c_str());
#endif
}
