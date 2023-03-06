// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Generic.hxx"
#include "Id3Scan.hxx"
#include "ApeTag.hxx"
#include "fs/Path.hxx"
#include "thread/Mutex.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "config.h"

bool
ScanGenericTags(InputStream &is, TagHandler &handler)
{
	if (!is.IsSeekable())
		return false;

	if (tag_ape_scan2(is, handler))
		return true;

#ifdef ENABLE_ID3TAG
	is.LockRewind();

	return tag_id3_scan(is, handler);
#else
	return false;
#endif
}

bool
ScanGenericTags(Path path, TagHandler &handler)
{
	Mutex mutex;

	auto is = OpenLocalInputStream(path, mutex);
	return ScanGenericTags(*is, handler);
}
