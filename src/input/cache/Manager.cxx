// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Manager.hxx"
#include "Config.hxx"
#include "Item.hxx"
#include "Lease.hxx"
#include "input/InputStream.hxx"
#include "fs/Traits.hxx"
#include "util/DeleteDisposer.hxx"

#include <string.h>

inline std::string_view
InputCacheManager::ItemGetUri::operator()(const InputCacheItem &item) const noexcept
{
	return item.GetUri();
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
InputCacheManager::Contains(std::string_view uri) noexcept
{
	return items_by_uri.find(uri) != items_by_uri.end();
}

inline InputCacheLease
InputCacheManager::Lookup(std::string_view uri) noexcept
{
	const auto iter = items_by_uri.find(uri);
	if (iter == items_by_uri.end())
		return {};

	auto &item = *iter;

	/* refresh */
	items_by_time.erase(items_by_time.iterator_to(item));
	items_by_time.push_back(item);

	// TODO revalidate the cache item using the file's mtime?
	// TODO if cache item contains error, retry now?

	return InputCacheLease{item};
}

inline InputCacheLease
InputCacheManager::Insert(InputCacheItem &item) noexcept
{
	total_size += item.size();

	while (total_size > max_total_size && EvictOldestUnused()) {}

	items_by_uri.insert(item);
	items_by_time.push_back(item);

	return InputCacheLease{item};
}

InputCacheLease
InputCacheManager::Get(std::string_view uri, bool create)
{
	// TODO: allow caching remote files
	if (!PathTraitsUTF8::IsAbsolute(uri))
		return {};

	if (auto lease = Lookup(uri))
		return lease;

	if (!create)
		return {};

	// TODO: wait for "ready" without blocking here
	auto is = InputStream::OpenReady(uri, stream_mutex);

	if (!IsEligible(*is))
		return {};

	return Insert(*new InputCacheItem(std::move(is)));
}

void
InputCacheManager::Prefetch(std::string_view uri)
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
