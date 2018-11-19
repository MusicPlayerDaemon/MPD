/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "InputStream.hxx"
#include "Handler.hxx"
#include "tag/Tag.hxx"
#include "util/ASCII.hxx"

#include <stdexcept>

#include <assert.h>

InputStream::~InputStream() noexcept
{
}

void
InputStream::Check()
{
}

void
InputStream::Update() noexcept
{
}

void
InputStream::SetReady() noexcept
{
	assert(!ready);

	ready = true;

	InvokeOnReady();
}

/**
 * Is seeking on resources behind this URI "expensive"?  For example,
 * seeking in a HTTP file requires opening a new connection with a new
 * HTTP request.
 */
gcc_pure
static bool
ExpensiveSeeking(const char *uri) noexcept
{
	return StringStartsWithCaseASCII(uri, "http://") ||
		StringStartsWithCaseASCII(uri, "tidal://") ||
		StringStartsWithCaseASCII(uri, "qobuz://") ||
		StringStartsWithCaseASCII(uri, "https://");
}

bool
InputStream::CheapSeeking() const noexcept
{
	return IsSeekable() && !ExpensiveSeeking(uri.c_str());
}

void
InputStream::Seek(gcc_unused offset_type new_offset)
{
	throw std::runtime_error("Seeking is not implemented");
}

void
InputStream::LockSeek(offset_type _offset)
{
	const std::lock_guard<Mutex> protect(mutex);
	Seek(_offset);
}

void
InputStream::LockSkip(offset_type _offset)
{
	const std::lock_guard<Mutex> protect(mutex);
	Skip(_offset);
}

std::unique_ptr<Tag>
InputStream::ReadTag()
{
	return nullptr;
}

std::unique_ptr<Tag>
InputStream::LockReadTag()
{
	const std::lock_guard<Mutex> protect(mutex);
	return ReadTag();
}

bool
InputStream::IsAvailable() noexcept
{
	return true;
}

size_t
InputStream::LockRead(void *ptr, size_t _size)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(ptr != nullptr);
#endif
	assert(_size > 0);

	const std::lock_guard<Mutex> protect(mutex);
	return Read(ptr, _size);
}

void
InputStream::ReadFull(void *_ptr, size_t _size)
{
	uint8_t *ptr = (uint8_t *)_ptr;

	size_t nbytes_total = 0;
	while (_size > 0) {
		size_t nbytes = Read(ptr + nbytes_total, _size);
		if (nbytes == 0)
			throw std::runtime_error("Unexpected end of file");

		nbytes_total += nbytes;
		_size -= nbytes;
	}
}

void
InputStream::LockReadFull(void *ptr, size_t _size)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(ptr != nullptr);
#endif
	assert(_size > 0);

	const std::lock_guard<Mutex> protect(mutex);
	ReadFull(ptr, _size);
}

bool
InputStream::LockIsEOF() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	return IsEOF();
}

void
InputStream::InvokeOnReady() noexcept
{
	if (handler != nullptr)
		handler->OnInputStreamReady();
}

void
InputStream::InvokeOnAvailable() noexcept
{
	if (handler != nullptr)
		handler->OnInputStreamAvailable();
}
