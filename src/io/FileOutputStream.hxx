// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef FILE_OUTPUT_STREAM_HXX
#define FILE_OUTPUT_STREAM_HXX

#include "OutputStream.hxx"
#include "fs/AllocatedPath.hxx"

#ifndef _WIN32
#include "io/FileDescriptor.hxx"
#endif

#include <cassert>
#include <cstdint>

#ifdef _WIN32
#include <fileapi.h>
#include <windef.h> // for HWND (needed by winbase.h)
#include <handleapi.h> // for INVALID_HANDLE_VALUE
#include <winbase.h> // for FILE_END
#endif

#if defined(__linux__) && !defined(ANDROID)
/* we don't use O_TMPFILE on Android because Android's braindead
   SELinux policy disallows hardlinks
   (https://android.googlesource.com/platform/external/sepolicy/+/85ce2c7),
   even hardlinks from /proc/self/fd/N, which however is required to
   use O_TMPFILE */
#define HAVE_O_TMPFILE
#endif

class Path;

/**
 * An #OutputStream implementation which writes to a file.
 *
 * The destructor will attempt to roll back the changes by calling
 * Cancel().  To confirm that data shall be written and the existing
 * file shall be replaced, call Commit().
 */
class FileOutputStream final : public OutputStream {
	const AllocatedPath path;

	/**
	 * If a temporary file is being written to, then this is its
	 * path.  Commit() will rename it to the path specified in the
	 * constructor.
	 */
	AllocatedPath tmp_path{nullptr};

#ifdef __linux__
	const FileDescriptor directory_fd;
#endif

#ifdef _WIN32
	HANDLE handle = INVALID_HANDLE_VALUE;
#else
	FileDescriptor fd = FileDescriptor::Undefined();
#endif

#ifdef HAVE_O_TMPFILE
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

#ifdef __linux__
	FileOutputStream(FileDescriptor _directory_fd, Path _path,
			 Mode _mode=Mode::CREATE);
#endif

	~FileOutputStream() noexcept {
		if (IsDefined())
			Cancel();
	}

	FileOutputStream(const FileOutputStream &) = delete;
	FileOutputStream &operator=(const FileOutputStream &) = delete;

public:
	Path GetPath() const noexcept {
		return path;
	}

	/**
	 * Returns the current offset.
	 */
	[[gnu::pure]]
	uint64_t Tell() const noexcept;

	/* virtual methods from class OutputStream */
	void Write(std::span<const std::byte> src) override;

	/**
	 * Flush all data written to this object to disk (but does not
	 * commit to the final path).  This method blocks until this
	 * flush is complete.  It can be called repeatedly.
	 *
	 * Throws on error.
	 */
	void Sync();

	/**
	 * Commit all data written to the file and make the file
	 * visible on the specified path.
	 *
	 * After returning, this object must not be used again.
	 *
	 * Throws on error.
	 */
	void Commit();

	/**
	 * Attempt to roll back all changes.
	 *
	 * After returning, this object must not be used again.
	 */
	void Cancel() noexcept;

private:
	void OpenCreate(bool visible);
	void OpenAppend(bool create);
	void Open();

	bool Close() noexcept {
		assert(IsDefined());

#ifdef _WIN32
		CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
		return true;
#else
		return fd.Close();
#endif
	}

#ifdef _WIN32
	bool SeekEOF() noexcept {
		return SetFilePointer(handle, 0, nullptr,
				      FILE_END) != 0xffffffff;
	}
#endif

	bool IsDefined() const noexcept {
#ifdef _WIN32
		return handle != INVALID_HANDLE_VALUE;
#else
		return fd.IsDefined();
#endif
	}

	void RenameOrThrow(Path old_path, Path new_path) const;
	void Delete(Path delete_path) const noexcept;
};

#endif
