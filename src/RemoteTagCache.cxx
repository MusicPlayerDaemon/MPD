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

#include "RemoteTagCache.hxx"
#include "RemoteTagCacheHandler.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "input/ScanTags.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

static constexpr Domain remote_tag_cache_domain("remote_tag_cache");

RemoteTagCache::RemoteTagCache(EventLoop &event_loop,
			       RemoteTagCacheHandler &_handler) noexcept
	:handler(_handler),
	 defer_invoke_handler(event_loop, BIND_THIS_METHOD(InvokeHandlers)),
	 map(typename KeyMap::bucket_traits(&buckets.front(), buckets.size()))
{
}

RemoteTagCache::~RemoteTagCache() noexcept
{
	map.clear_and_dispose(DeleteDisposer());
}

void
RemoteTagCache::Lookup(const std::string &uri) noexcept
{
	std::unique_lock<Mutex> lock(mutex);

	KeyMap::insert_commit_data hint;
	auto [tag, value] = map.insert_check(uri, Item::Hash(), Item::Equal(), hint);
	if (value) {
		auto item = new Item(*this, uri);
		map.insert_commit(*item, hint);
		waiting_list.push_back(*item);
		lock.unlock();

		try {
			item->scanner = InputScanTags(uri.c_str(), *item);
			if (!item->scanner) {
				/* unsupported */
				lock.lock();
				ItemResolved(*item);
				return;
			}

			item->scanner->Start();
		} catch (...) {
			FmtError(remote_tag_cache_domain,
				 "Failed to scan tags of '{}': {}",
				 uri, std::current_exception());

			item->scanner.reset();

			lock.lock();
			ItemResolved(*item);
			return;
		}
	} else if (tag->scanner) {
		/* already scanning this one - no-op */
	} else {
		/* already finished: re-invoke the handler */

		idle_list.erase(waiting_list.iterator_to(*tag));
		invoke_list.push_back(*tag);

		ScheduleInvokeHandlers();
	}
}

void
RemoteTagCache::ItemResolved(Item &item) noexcept
{
	waiting_list.erase(waiting_list.iterator_to(item));
	invoke_list.push_back(item);

	ScheduleInvokeHandlers();
}

void
RemoteTagCache::InvokeHandlers() noexcept
{
	const std::scoped_lock<Mutex> lock(mutex);

	while (!invoke_list.empty()) {
		auto &item = invoke_list.front();
		invoke_list.pop_front();
		idle_list.push_back(item);

		const ScopeUnlock unlock(mutex);
		handler.OnRemoteTag(item.uri.c_str(), item.tag);
	}

	/* evict items if there are too many */
	while (map.size() > MAX_SIZE && !idle_list.empty()) {
		auto *item = &idle_list.front();
		idle_list.pop_front();
		map.erase(map.iterator_to(*item));
		delete item;
	}
}

void
RemoteTagCache::Item::OnRemoteTag(Tag &&_tag) noexcept
{
	tag = std::move(_tag);

	scanner.reset();

	const std::scoped_lock<Mutex> lock(parent.mutex);
	parent.ItemResolved(*this);
}

void
RemoteTagCache::Item::OnRemoteTagError(std::exception_ptr e) noexcept
{
	FmtError(remote_tag_cache_domain,
		 "Failed to scan tags of '{}': {}", uri, e);

	scanner.reset();

	const std::scoped_lock<Mutex> lock(parent.mutex);
	parent.ItemResolved(*this);
}
