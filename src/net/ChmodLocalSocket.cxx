// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "ChmodLocalSocket.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "system/Error.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <fcntl.h> // for AT_*
#include <sys/stat.h>

using std::string_view_literals::operator""sv;

static bool
IsSocket(FileDescriptor fd, const char *path)
{
	struct statx stx;
	if (statx(fd.Get(), "",
		  AT_EMPTY_PATH|AT_NO_AUTOMOUNT|AT_SYMLINK_NOFOLLOW|AT_STATX_DONT_SYNC,
		  STATX_TYPE, &stx) < 0)
		throw FmtErrno("statx({:?}) failed"sv, path);

	return S_ISSOCK(stx.stx_mode);
}

void
ChmodLocalSocket(const char *path, mode_t mode)
{
#ifdef __linux__
	/* note: this O_NOFOLLOW affects only the last path component;
	   symlinks in the middle are followed, and that alone can
	   pose a risk, not mitigated here */
	const auto fd = OpenPath(path, O_NOFOLLOW);

	/* is this really a socket? */
	if (!IsSocket(fd, path))
		throw FmtRuntimeError("Not a socket: {:?}", path);

	/* note: fchmod() doesn't work on O_PATH file descriptors, but
	   fchmodat() with AT_EMPTY_PATH workw */
	if (fchmodat(fd.Get(), "", mode, AT_EMPTY_PATH) < 0)
		throw FmtErrno("chmod({:?}) failed"sv, path);
#else
	/* portable fallback */
	if (chmod(path, mode) < 0)
		throw FmtErrno("chmod({:?}) failed"sv, path);
#endif
}
