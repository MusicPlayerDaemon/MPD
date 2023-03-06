// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LocalOpen.hxx"
#include "InputStream.hxx"
#include "plugins/FileInputPlugin.hxx"
#include "config.h"

#include "io/uring/Features.h"
#ifdef HAVE_URING
#include "plugins/UringInputPlugin.hxx"
#endif

#ifdef ENABLE_ARCHIVE
#include "plugins/ArchiveInputPlugin.hxx"
#endif

#include "fs/Path.hxx"
#include "system/Error.hxx"

#include <cassert>

InputStreamPtr
OpenLocalInputStream(Path path, Mutex &mutex)
{
	InputStreamPtr is;

#ifdef ENABLE_ARCHIVE
	try {
#endif
#ifdef HAVE_URING
		is = OpenUringInputStream(path.c_str(), mutex);
		if (is)
			return is;
#endif

		is = OpenFileInputStream(path, mutex);
#ifdef ENABLE_ARCHIVE
	} catch (const std::system_error &e) {
		if (IsPathNotFound(e)) {
			/* ENOTDIR means this may be a path inside an archive
			   file */
			is = OpenArchiveInputStream(path, mutex);
			if (!is)
				throw;
		} else
			throw;
	}
#endif

	assert(is);
	assert(is->IsReady());

	return is;
}
