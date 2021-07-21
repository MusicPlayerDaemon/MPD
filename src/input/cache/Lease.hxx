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

#ifndef MPD_INPUT_CACHE_LEASE_HXX
#define MPD_INPUT_CACHE_LEASE_HXX

#include "Item.hxx"

#include <boost/intrusive/list_hook.hpp>

#include <utility>

/**
 * A lease for an #InputCacheItem.
 */
class InputCacheLease
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>
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
