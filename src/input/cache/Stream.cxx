/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Stream.hxx"

CacheInputStream::CacheInputStream(InputCacheLease _lease,
				   Mutex &_mutex) noexcept
	:InputStream(_lease->GetUri(), _mutex),
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
	const std::scoped_lock<Mutex> protect(i.mutex);

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
	const std::scoped_lock<Mutex> protect(i.mutex);

	return i.IsAvailable(_offset);
}

size_t
CacheInputStream::Read(std::unique_lock<Mutex> &lock,
		       void *ptr, size_t read_size)
{
	const auto _offset = offset;
	auto &i = GetCacheItem();

	size_t nbytes;

	{
		const ScopeUnlock unlock(mutex);
		const std::scoped_lock<Mutex> protect(i.mutex);

		nbytes = i.Read(lock, _offset, ptr, read_size);
	}

	offset += nbytes;
	return nbytes;
}

void
CacheInputStream::OnInputCacheAvailable() noexcept
{
	auto &i = GetCacheItem();
	const ScopeUnlock unlock(i.mutex);

	const std::scoped_lock<Mutex> protect(mutex);
	InvokeOnAvailable();
}
