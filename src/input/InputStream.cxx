/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "InputStream.hxx"
#include "thread/Cond.hxx"
#include "util/StringCompare.hxx"

#include <assert.h>

InputStream::~InputStream()
{
}

bool
InputStream::Check(gcc_unused Error &error)
{
	return true;
}

void
InputStream::Update()
{
}

void
InputStream::SetReady()
{
	assert(!ready);

	ready = true;
	cond.broadcast();
}

void
InputStream::WaitReady()
{
	while (true) {
		Update();
		if (ready)
			break;

		cond.wait(mutex);
	}
}

void
InputStream::LockWaitReady()
{
	const ScopeLock protect(mutex);
	WaitReady();
}

/**
 * Is seeking on resources behind this URI "expensive"?  For example,
 * seeking in a HTTP file requires opening a new connection with a new
 * HTTP request.
 */
gcc_pure
static bool
ExpensiveSeeking(const char *uri)
{
	return StringStartsWith(uri, "http://") ||
		StringStartsWith(uri, "https://");
}

bool
InputStream::CheapSeeking() const
{
	return IsSeekable() && !ExpensiveSeeking(uri.c_str());
}

bool
InputStream::Seek(gcc_unused offset_type new_offset,
		  gcc_unused Error &error)
{
	return false;
}

bool
InputStream::LockSeek(offset_type _offset, Error &error)
{
	const ScopeLock protect(mutex);
	return Seek(_offset, error);
}

bool
InputStream::LockSkip(offset_type _offset, Error &error)
{
	const ScopeLock protect(mutex);
	return Skip(_offset, error);
}

Tag *
InputStream::ReadTag()
{
	return nullptr;
}

Tag *
InputStream::LockReadTag()
{
	const ScopeLock protect(mutex);
	return ReadTag();
}

bool
InputStream::IsAvailable()
{
	return true;
}

size_t
InputStream::LockRead(void *ptr, size_t _size, Error &error)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(ptr != nullptr);
#endif
	assert(_size > 0);

	const ScopeLock protect(mutex);
	return Read(ptr, _size, error);
}

bool
InputStream::ReadFull(void *_ptr, size_t _size, Error &error)
{
	uint8_t *ptr = (uint8_t *)_ptr;

	size_t nbytes_total = 0;
	while (_size > 0) {
		size_t nbytes = Read(ptr + nbytes_total, _size, error);
		if (nbytes == 0)
			return false;

		nbytes_total += nbytes;
		_size -= nbytes;
	}
	return true;
}

bool
InputStream::LockReadFull(void *ptr, size_t _size, Error &error)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(ptr != nullptr);
#endif
	assert(_size > 0);

	const ScopeLock protect(mutex);
	return ReadFull(ptr, _size, error);
}

bool
InputStream::LockIsEOF()
{
	const ScopeLock protect(mutex);
	return IsEOF();
}
