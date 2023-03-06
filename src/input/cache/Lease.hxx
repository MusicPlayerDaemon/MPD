// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_CACHE_LEASE_HXX
#define MPD_INPUT_CACHE_LEASE_HXX

#include "Item.hxx"
#include "util/IntrusiveList.hxx"

#include <utility>

/**
 * A lease for an #InputCacheItem.
 */
class InputCacheLease
	: public IntrusiveListHook<>
{
	InputCacheItem *item = nullptr;

public:
	InputCacheLease() = default;

	explicit InputCacheLease(InputCacheItem &_item) noexcept
		:item(&_item)
	{
		item->AddLease(*this);
	}

	InputCacheLease(InputCacheLease &&src) noexcept
		:item(std::exchange(src.item, nullptr))
	{
		if (item != nullptr) {
			item->RemoveLease(src);
			item->AddLease(*this);
		}
	}

	~InputCacheLease() noexcept {
		if (item != nullptr)
			item->RemoveLease(*this);
	}

	InputCacheLease &operator=(InputCacheLease &&src) noexcept {
		using std::swap;
		swap(item, src.item);

		if (item != nullptr) {
			item->RemoveLease(src);
			item->AddLease(*this);
		}

		return *this;
	}

	operator bool() const noexcept {
		return item != nullptr;
	}

	auto &operator*() const noexcept {
		return *item;
	}

	auto *operator->() const noexcept {
		return item;
	}

	auto &GetCacheItem() const noexcept {
		return *item;
	}

	/**
	 * Caller locks #InputCacheItem::mutex.
	 */
	virtual void OnInputCacheAvailable() noexcept {}
};

#endif
