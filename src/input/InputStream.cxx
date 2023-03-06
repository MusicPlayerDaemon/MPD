// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "InputStream.hxx"
#include "Handler.hxx"
#include "tag/Tag.hxx"
#include "util/ASCII.hxx"

#include <cassert>
#include <stdexcept>

InputStream::~InputStream() noexcept = default;

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
[[gnu::pure]]
static bool
ExpensiveSeeking(const char *uri) noexcept
{
	return StringStartsWithCaseASCII(uri, "http://") ||
		StringStartsWithCaseASCII(uri, "qobuz://") ||
		StringStartsWithCaseASCII(uri, "https://");
}

bool
InputStream::CheapSeeking() const noexcept
{
	return IsSeekable() && !ExpensiveSeeking(uri.c_str());
}

//[[noreturn]]
void
InputStream::Seek(std::unique_lock<Mutex> &, [[maybe_unused]] offset_type new_offset)
{
	throw std::runtime_error("Seeking is not implemented");
}

void
InputStream::LockSeek(offset_type _offset)
{
	std::unique_lock<Mutex> lock(mutex);
	Seek(lock, _offset);
}

void
InputStream::LockSkip(offset_type _offset)
{
	std::unique_lock<Mutex> lock(mutex);
	Skip(lock, _offset);
}

std::unique_ptr<Tag>
InputStream::ReadTag() noexcept
{
	return nullptr;
}

std::unique_ptr<Tag>
InputStream::LockReadTag() noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);
	return ReadTag();
}

bool
InputStream::IsAvailable() const noexcept
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

	std::unique_lock<Mutex> lock(mutex);
	return Read(lock, ptr, _size);
}

void
InputStream::ReadFull(std::unique_lock<Mutex> &lock, void *_ptr, size_t _size)
{
	auto *ptr = (uint8_t *)_ptr;

	size_t nbytes_total = 0;
	while (_size > 0) {
		size_t nbytes = Read(lock, ptr + nbytes_total, _size);
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

	std::unique_lock<Mutex> lock(mutex);
	ReadFull(lock, ptr, _size);
}

bool
InputStream::LockIsEOF() const noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);
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
