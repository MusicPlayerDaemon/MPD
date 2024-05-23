// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
		const std::scoped_lock lock{mutex};
		return !leases.empty();
	}

	void AddLease(InputCacheLease &lease) noexcept;
	void RemoveLease(InputCacheLease &lease) noexcept;

private:
	/* virtual methods from class BufferingInputStream */
	void OnBufferAvailable() noexcept override;
};

#endif
