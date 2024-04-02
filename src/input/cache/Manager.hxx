// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_CACHE_MANAGER_HXX
#define MPD_INPUT_CACHE_MANAGER_HXX

#include "thread/Mutex.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/IntrusiveList.hxx"

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

	struct ItemGetUri {
		[[gnu::pure]]
		std::string_view operator()(const InputCacheItem &item) const noexcept;
	};

	IntrusiveList<InputCacheItem> items_by_time;

	IntrusiveHashSet<InputCacheItem, 127,
			 IntrusiveHashSetOperators<InputCacheItem, ItemGetUri,
						   std::hash<std::string_view>,
						   std::equal_to<std::string_view>>> items_by_uri;

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
