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

#include "config.h"
#include "Generic.hxx"
#include "Id3Scan.hxx"
#include "ApeTag.hxx"
#include "fs/Path.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "Log.hxx"

#include <stdexcept>

bool
ScanGenericTags(InputStream &is, const TagHandler &handler, void *ctx)
{
	if (tag_ape_scan2(is, handler, ctx))
		return true;

#ifdef ENABLE_ID3TAG
	try {
		is.LockRewind();
	} catch (const std::runtime_error &) {
		return false;
	}

	return tag_id3_scan(is, handler, ctx);
#else
	return false;
#endif
}

bool
ScanGenericTags(Path path, const TagHandler &handler, void *ctx)
try {
	Mutex mutex;
	Cond cond;

	auto is = OpenLocalInputStream(path, mutex, cond);
	return ScanGenericTags(*is, handler, ctx);
} catch (const std::runtime_error &e) {
	LogError(e);
	return false;
}
