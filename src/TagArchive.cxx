// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TagArchive.hxx"
#include "TagStream.hxx"
#include "archive/ArchiveFile.hxx"
#include "input/InputStream.hxx"

bool
tag_archive_scan(ArchiveFile &archive, const char *path_utf8,
		 TagHandler &handler) noexcept
try {
	Mutex mutex;

	auto is = archive.OpenStream(path_utf8, mutex);
	if (!is)
		return false;

	return tag_stream_scan(*is, handler);
} catch (...) {
	return false;
}

bool
tag_archive_scan(ArchiveFile &archive, const char *path_utf8,
		 TagBuilder &builder) noexcept
try {
	Mutex mutex;

	auto is = archive.OpenStream(path_utf8, mutex);
	return is && tag_stream_scan(*is, builder);
} catch (...) {
	return false;
}
