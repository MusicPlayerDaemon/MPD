/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#ifndef MPD_INPUT_CACHE_ITEM_HXX
#define MPD_INPUT_CACHE_ITEM_HXX

#include "input/BufferingInputStream.hxx"
#include "thread/Mutex.hxx"
#include "util/IntrusiveList.hxx"
#include "util/IntrusiveHashSet.hxx"

#include <string>

class InputCacheLease;

/**
 * An item in the #InputCacheManager.  It caches the contents of a
 * file, and reading and managing it through the base class
 * #BufferingInputStream.
 *
 * Use the class #CacheInputStream to read from it.
 */
class InputCacheItem final
	: public BufferingInputStream,
	  public AutoUnlinkIntrusiveListHook,
	  public IntrusiveHashSetHook<>
{
	const std::string uri;

	using LeaseList = IntrusiveList<InputCacheLease>;

	LeaseList leases;
	LeaseList::iterator next_lease = leases.end();

public:
	explicit InputCacheItem(InputStreamPtr _input) noexcept;
	~InputCacheItem() noexcept;

	const std::string &GetUri() const noexcept {
		return uri;
	}

	using BufferingInputStream::size;

	bool IsInUse() const noexcept {
		const std::scoped_lock<Mutex> lock(mutex);
		return !leases.empty();
	}

	void AddLease(InputCacheLease &lease) noexcept;
	void RemoveLease(InputCacheLease &lease) noexcept;

private:
	/* virtual methods from class BufferingInputStream */
	void OnBufferAvailable() noexcept override;
};

#endif
