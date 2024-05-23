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
	std::unique_lock lock{mutex};
	Seek(lock, _offset);
}

void
InputStream::LockSkip(offset_type _offset)
{
	std::unique_lock lock{mutex};
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
	const std::scoped_lock protect{mutex};
	return ReadTag();
}

bool
InputStream::IsAvailable() const noexcept
{
	return true;
}

size_t
InputStream::LockRead(std::span<std::byte> dest)
{
	assert(!dest.empty());

	std::unique_lock lock{mutex};
	return Read(lock, dest);
}

void
InputStream::ReadFull(std::unique_lock<Mutex> &lock, std::span<std::byte> dest)
{
	assert(!dest.empty());

	do {
		std::size_t nbytes = Read(lock, dest);
		if (nbytes == 0)
			throw std::runtime_error("Unexpected end of file");

		dest = dest.subspan(nbytes);
	} while (!dest.empty());
}

void
InputStream::LockReadFull(std::span<std::byte> dest)
{
	assert(!dest.empty());

	std::unique_lock lock{mutex};
	ReadFull(lock, dest);
}

bool
InputStream::LockIsEOF() const noexcept
{
	const std::scoped_lock protect{mutex};
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
