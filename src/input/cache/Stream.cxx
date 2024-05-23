// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Stream.hxx"

CacheInputStream::CacheInputStream(InputCacheLease _lease,
				   Mutex &_mutex) noexcept
	:InputStream(_lease->GetUri().c_str(), _mutex),
	 InputCacheLease(std::move(_lease))
{
	const auto &i = GetCacheItem();
	size = i.size();
	seekable = true;
	SetReady();
}

void
CacheInputStream::Check()
{
	const ScopeUnlock unlock(mutex);

	auto &i = GetCacheItem();
	const std::scoped_lock protect{i.mutex};

	i.Check();
}

void
CacheInputStream::Seek(std::unique_lock<Mutex> &, offset_type new_offset)
{
	offset = new_offset;
}

bool
CacheInputStream::IsEOF() const noexcept
{
	return offset == size;
}

bool
CacheInputStream::IsAvailable() const noexcept
{
	const auto _offset = offset;
	const ScopeUnlock unlock(mutex);

	auto &i = GetCacheItem();
	const std::scoped_lock protect{i.mutex};

	return i.IsAvailable(_offset);
}

size_t
CacheInputStream::Read(std::unique_lock<Mutex> &lock,
		       std::span<std::byte> dest)
{
	const auto _offset = offset;
	auto &i = GetCacheItem();

	size_t nbytes;

	{
		const ScopeUnlock unlock(mutex);
		const std::scoped_lock protect{i.mutex};

		nbytes = i.Read(lock, _offset, dest);
	}

	offset += nbytes;
	return nbytes;
}

void
CacheInputStream::OnInputCacheAvailable() noexcept
{
	auto &i = GetCacheItem();
	const ScopeUnlock unlock(i.mutex);

	const std::scoped_lock protect{mutex};
	InvokeOnAvailable();
}
