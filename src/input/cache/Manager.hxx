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

#ifndef MPD_INPUT_CACHE_MANAGER_HXX
#define MPD_INPUT_CACHE_MANAGER_HXX

#include "thread/Mutex.hxx"

#include <boost/intrusive/set.hpp>
#include <boost/intrusive/list.hpp>

class InputStream;
class InputCacheItem;
class InputCacheLease;
struct InputCacheConfig;

/**
 * A class which caches files in RAM.  It is supposed to prefetch
 * files before they are played.
 */
class InputCacheManager {
	const size_t max_total_size;

	mutable Mutex mutex;

	size_t total_size = 0;

	struct ItemCompare {
		[[gnu::pure]]
		bool operator()(const InputCacheItem &a,
				const char *b) const noexcept;

		[[gnu::pure]]
		bool operator()(const char *a,
				const InputCacheItem &b) const noexcept;

		[[gnu::pure]]
		bool operator()(const InputCacheItem &a,
				const InputCacheItem &b) const noexcept;
	};

	boost::intrusive::list<InputCacheItem,
			       boost::intrusive::base_hook<boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>>,
			       boost::intrusive::constant_time_size<false>> items_by_time;

	using UriMap =
		boost::intrusive::set<InputCacheItem,
				      boost::intrusive::base_hook<boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>>,
				      boost::intrusive::compare<ItemCompare>,
				      boost::intrusive::constant_time_size<false>>;

	UriMap items_by_uri;

public:
	explicit InputCacheManager(const InputCacheConfig &config) noexcept;
	~InputCacheManager() noexcept;

	void Flush() noexcept;

	[[gnu::pure]]
	bool Contains(const char *uri) noexcept;

	/**
	 * Throws if opening the #InputStream fails.
	 *
	 * @param create if true, then the cache item will be created
	 * if it did not exist
	 * @return a lease of the new item or nullptr if the file is
	 * not eligible for caching
	 */
	InputCacheLease Get(const char *uri, bool create);

	/**
	 * Shortcut for "Get(uri,true)", discarding the returned
	 * lease.
	 */
	void Prefetch(const char *uri);

private:
	/**
	 * Check whether the given #InputStream can be stored in this
	 * cache.
	 */
	bool IsEligible(const InputStream &input) const noexcept;

	void Remove(InputCacheItem &item) noexcept;
	void Delete(InputCacheItem *item) noexcept;

	InputCacheItem *FindOldestUnused() noexcept;

	/**
	 * @return true if one item has been evicted, false if no
	 * unused item was found
	 */
	bool EvictOldestUnused() noexcept;
};

#endif
