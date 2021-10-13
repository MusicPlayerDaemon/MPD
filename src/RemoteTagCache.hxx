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

#ifndef MPD_REMOTE_TAG_CACHE_HXX
#define MPD_REMOTE_TAG_CACHE_HXX

#include "input/RemoteTagScanner.hxx"
#include "tag/Tag.hxx"
#include "event/InjectEvent.hxx"
#include "thread/Mutex.hxx"

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/unordered_set.hpp>

#include <string>

class RemoteTagCacheHandler;

/**
 * A cache for tags received via #RemoteTagScanner.
 */
class RemoteTagCache final {
	static constexpr size_t MAX_SIZE = 4096;

	RemoteTagCacheHandler &handler;

	InjectEvent defer_invoke_handler;

	Mutex mutex;

	struct Item final
		: public boost::intrusive::unordered_set_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
		  public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>,
		  RemoteTagHandler
	{
		RemoteTagCache &parent;

		const std::string uri;

		std::unique_ptr<RemoteTagScanner> scanner;

		Tag tag;

		template<typename U>
		Item(RemoteTagCache &_parent, U &&_uri) noexcept
			:parent(_parent), uri(std::forward<U>(_uri)) {}

		/* virtual methods from RemoteTagHandler */
		void OnRemoteTag(Tag &&tag) noexcept override;
		void OnRemoteTagError(std::exception_ptr e) noexcept override;

		struct Hash : std::hash<std::string> {
			using std::hash<std::string>::operator();

			[[gnu::pure]]
			std::size_t operator()(const Item &item) const noexcept {
				return std::hash<std::string>::operator()(item.uri);
			}
		};

		struct Equal {
			[[gnu::pure]]
			bool operator()(const Item &a,
					const Item &b) const noexcept {
				return a.uri == b.uri;
			}

			[[gnu::pure]]
			bool operator()(const std::string &a,
					const Item &b) const noexcept {
				return a == b.uri;
			}
		};
	};

	typedef boost::intrusive::list<Item,
				       boost::intrusive::constant_time_size<false>> ItemList;

	/**
	 * These items have been resolved completely (successful or
	 * failed).  All callbacks have been invoked.  The oldest
	 * comes first in the list, and is the first one to be evicted
	 * if the cache is full.
	 */
	ItemList idle_list;

	/**
	 * A #RemoteTagScanner instances is currently busy on fetching
	 * information, and we're waiting for our #RemoteTagHandler
	 * methods to be invoked.
	 */
	ItemList waiting_list;

	/**
	 * These items have just been resolved, and the
	 * #RemoteTagCacheHandler is about to be invoked.  After that,
	 * they will be moved to the #idle_list.
	 */
	ItemList invoke_list;

	typedef boost::intrusive::unordered_set<Item,
						boost::intrusive::hash<Item::Hash>,
						boost::intrusive::equal<Item::Equal>,
						boost::intrusive::constant_time_size<true>> KeyMap;

	std::array<typename KeyMap::bucket_type, 127> buckets;

	KeyMap map;

public:
	RemoteTagCache(EventLoop &event_loop,
		       RemoteTagCacheHandler &_handler) noexcept;
	~RemoteTagCache() noexcept;

	void Lookup(const std::string &uri) noexcept;

private:
	void InvokeHandlers() noexcept;

	void ScheduleInvokeHandlers() noexcept {
		defer_invoke_handler.Schedule();
	}

	void ItemResolved(Item &item) noexcept;
};

#endif
