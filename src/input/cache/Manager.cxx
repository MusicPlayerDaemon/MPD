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

#include "Manager.hxx"
#include "Config.hxx"
#include "Item.hxx"
#include "Lease.hxx"
#include "input/InputStream.hxx"
#include "fs/Traits.hxx"
#include "util/DeleteDisposer.hxx"

#include <string.h>

inline bool
InputCacheManager::ItemCompare::operator()(const InputCacheItem &a,
					   const char *b) const noexcept
{
	return strcmp(a.GetUri(), b) < 0;
}

inline bool
InputCacheManager::ItemCompare::operator()(const char *a,
					   const InputCacheItem &b) const noexcept
{
	return strcmp(a, b.GetUri()) < 0;
}

inline bool
InputCacheManager::ItemCompare::operator()(const InputCacheItem &a,
					   const InputCacheItem &b) const noexcept
{
	return strcmp(a.GetUri(), b.GetUri()) < 0;
}

InputCacheManager::InputCacheManager(const InputCacheConfig &config) noexcept
	:max_total_size(config.size)
{
}

InputCacheManager::~InputCacheManager() noexcept
{
	items_by_time.clear_and_dispose(DeleteDisposer());
}

void
InputCacheManager::Flush() noexcept
{
	items_by_time.remove_and_dispose_if([](const InputCacheItem &item){
		return !item.IsInUse();
	}, [this](InputCacheItem *item){
		// TODO: eliminate code duplication, see method Remove()
		assert(total_size >= item->size());
		total_size -= item->size();
		items_by_uri.erase(items_by_uri.iterator_to(*item));
		delete item;
	});

	// TODO: invalidate busy items and flush them later
}

bool
InputCacheManager::IsEligible(const InputStream &input) const noexcept
{
	assert(input.IsReady());

	return input.IsSeekable() && input.KnownSize() &&
		input.GetSize() > 0 &&
		input.GetSize() <= max_total_size / 2;
}

bool
InputCacheManager::Contains(const char *uri) noexcept
{
	return Get(uri, false);
}

InputCacheLease
InputCacheManager::Get(const char *uri, bool create)
{
	// TODO: allow caching remote files
	if (!PathTraitsUTF8::IsAbsolute(uri))
		return {};

	auto iter = items_by_uri.find(uri, items_by_uri.key_comp());
	if (iter != items_by_uri.end()) {
		auto &item = *iter;

		/* refresh */
		items_by_time.erase(items_by_time.iterator_to(item));
		items_by_time.push_back(item);

		// TODO revalidate the cache item using the file's mtime?
		// TODO if cache item contains error, retry now?

		return InputCacheLease(item);
	}

	if (!create)
		return {};

	// TODO: wait for "ready" without blocking here
	auto is = InputStream::OpenReady(uri, mutex);

	if (!IsEligible(*is))
		return {};

	const size_t size = is->GetSize();
	total_size += size;

	while (total_size > max_total_size && EvictOldestUnused()) {}

	auto *item = new InputCacheItem(std::move(is));
	items_by_uri.insert(*item);
	items_by_time.push_back(*item);

	return InputCacheLease(*item);
}

void
InputCacheManager::Prefetch(const char *uri)
{
	Get(uri, true);
}

void
InputCacheManager::Remove(InputCacheItem &item) noexcept
{
	assert(total_size >= item.size());
	total_size -= item.size();

	items_by_time.erase(items_by_time.iterator_to(item));
	items_by_uri.erase(items_by_uri.iterator_to(item));
}

void
InputCacheManager::Delete(InputCacheItem *item) noexcept
{
	Remove(*item);
	delete item;
}

InputCacheItem *
InputCacheManager::FindOldestUnused() noexcept
{
	for (auto &i : items_by_time)
		if (!i.IsInUse())
			return &i;

	return nullptr;
}

bool
InputCacheManager::EvictOldestUnused() noexcept
{
	auto *item = FindOldestUnused();
	if (item == nullptr)
		return false;

	Delete(item);
	return true;
}
