// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_REMOTE_TAG_CACHE_HXX
#define MPD_REMOTE_TAG_CACHE_HXX

#include "input/RemoteTagScanner.hxx"
#include "tag/Tag.hxx"
#include "event/InjectEvent.hxx"
#include "thread/Mutex.hxx"
#include "util/IntrusiveList.hxx"
#include "util/IntrusiveHashSet.hxx"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>

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
		: public IntrusiveHashSetHook<>,
		  public IntrusiveListHook<>,
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

		struct GetUri {
			[[gnu::pure]]
			std::string_view operator()(const Item &item) const noexcept {
				return item.uri;
			}
		};
	};

	using ItemList = IntrusiveList<Item>;

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

	IntrusiveHashSet<Item, 127,
			 IntrusiveHashSetOperators<std::hash<std::string_view>,
						   std::equal_to<std::string_view>,
						   Item::GetUri>,
			 IntrusiveHashSetBaseHookTraits<Item>,
			 true> map;

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
