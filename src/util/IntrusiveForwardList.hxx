// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Cast.hxx"
#include "Concepts.hxx"
#include "MemberPointer.hxx"
#include "OptionalCounter.hxx"
#include "OptionalField.hxx"
#include "ShallowCopy.hxx"

#include <iterator>
#include <type_traits>
#include <utility>

struct IntrusiveForwardListOptions {
	bool constant_time_size = false;

	/**
	 * Cache a pointer to the last item?  This makes back() and
	 * push_back() run in constant time.
	 */
	bool cache_last = false;
};

struct IntrusiveForwardListNode {
	IntrusiveForwardListNode *next;
};

struct IntrusiveForwardListHook {
	IntrusiveForwardListNode siblings;

	static constexpr auto &Cast(IntrusiveForwardListNode &node) noexcept {
		return ContainerCast(node, &IntrusiveForwardListHook::siblings);
	}

	static constexpr const auto &Cast(const IntrusiveForwardListNode &node) noexcept {
		return ContainerCast(node, &IntrusiveForwardListHook::siblings);
	}
};

/**
 * For classes which embed #IntrusiveForwardListHook as base class.
 */
template<typename T>
struct IntrusiveForwardListBaseHookTraits {
	static constexpr T *Cast(IntrusiveForwardListNode *node) noexcept {
		static_assert(std::is_base_of_v<IntrusiveForwardListHook, T>);
		auto *hook = &IntrusiveForwardListHook::Cast(*node);
		return static_cast<T *>(hook);
	}

	static constexpr const T *Cast(const IntrusiveForwardListNode *node) noexcept {
		static_assert(std::is_base_of_v<IntrusiveForwardListHook, T>);
		const auto *hook = &IntrusiveForwardListHook::Cast(*node);
		return static_cast<const T *>(hook);
	}

	static constexpr IntrusiveForwardListHook &ToHook(T &t) noexcept {
		static_assert(std::is_base_of_v<IntrusiveForwardListHook, T>);
		return t;
	}

	static constexpr const IntrusiveForwardListHook &ToHook(const T &t) noexcept {
		static_assert(std::is_base_of_v<IntrusiveForwardListHook, T>);
		return t;
	}
};

/**
 * For classes which embed #IntrusiveForwardListHook as member.
 */
template<auto member>
struct IntrusiveForwardListMemberHookTraits {
	using T = MemberPointerContainerType<decltype(member)>;
	using Hook = IntrusiveForwardListHook;

	static_assert(std::is_same_v<MemberPointerType<decltype(member)>, Hook>);

	static constexpr T *Cast(IntrusiveForwardListNode *node) noexcept {
		auto &hook = Hook::Cast(*node);
		return &ContainerCast(hook, member);
	}

	static constexpr const T *Cast(const IntrusiveForwardListNode *node) noexcept {
		const auto &hook = Hook::Cast(*node);
		return &ContainerCast(hook, member);
	}

	static constexpr auto &ToHook(T &t) noexcept {
		return t.*member;
	}

	static constexpr const auto &ToHook(const T &t) noexcept {
		return t.*member;
	}
};

/**
 * @param constant_time_size make size() constant-time by caching the
 * number of items in a field?
 */
template<typename T,
	 typename HookTraits=IntrusiveForwardListBaseHookTraits<T>,
	 IntrusiveForwardListOptions options=IntrusiveForwardListOptions{}>
class IntrusiveForwardList {
	static constexpr bool constant_time_size = options.constant_time_size;

	IntrusiveForwardListNode head{nullptr};

	[[no_unique_address]]
	OptionalField<IntrusiveForwardListNode *, options.cache_last> last_cache{&head};

	[[no_unique_address]]
	OptionalCounter<constant_time_size> counter;

	static constexpr T *Cast(IntrusiveForwardListNode *node) noexcept {
		return HookTraits::Cast(node);
	}

	static constexpr const T *Cast(const IntrusiveForwardListNode *node) noexcept {
		return HookTraits::Cast(node);
	}

	static constexpr IntrusiveForwardListHook &ToHook(T &t) noexcept {
		return HookTraits::ToHook(t);
	}

	static constexpr const IntrusiveForwardListHook &ToHook(const T &t) noexcept {
		return HookTraits::ToHook(t);
	}

	static constexpr IntrusiveForwardListNode &ToNode(T &t) noexcept {
		return ToHook(t).siblings;
	}

	static constexpr const IntrusiveForwardListNode &ToNode(const T &t) noexcept {
		return ToHook(t).siblings;
	}

public:
	using value_type = T;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	using size_type = std::size_t;

	IntrusiveForwardList() = default;

	IntrusiveForwardList(IntrusiveForwardList &&src) noexcept
		:head{std::exchange(src.head.next, nullptr)}
	{
		using std::swap;
		swap(last_cache, src.last_cache);
		swap(counter, src.counter);
	}

	constexpr IntrusiveForwardList(ShallowCopy, const IntrusiveForwardList &src) noexcept
		:head(src.head)
	{
		// shallow copies mess with the counter
		static_assert(!options.constant_time_size);
		static_assert(!options.cache_last);
	}

	IntrusiveForwardList &operator=(IntrusiveForwardList &&src) noexcept {
		using std::swap;
		swap(head, src.head);
		swap(last_cache, src.last_cache);
		swap(counter, src.counter);
		return *this;
	}

	constexpr bool empty() const noexcept {
		return head.next == nullptr;
	}

	constexpr size_type size() const noexcept
		requires(constant_time_size) {
		return counter;
	}

	void clear() noexcept {
		head = {};
		last_cache = {&head};
		counter.reset();
	}

	void clear_and_dispose(Disposer<value_type> auto disposer) noexcept {
		while (!empty()) {
			auto *item = &front();
			pop_front();
			disposer(item);
		}

		last_cache = {&head};
	}

	/**
	 * @return the number of removed items
	 */
	std::size_t remove_and_dispose_if(std::predicate<const_reference> auto pred,
					  Disposer<value_type> auto dispose) noexcept {
		std::size_t result = 0;

		for (auto prev = before_begin(), current = std::next(prev);
		     current != end();) {
			auto &item = *current;

			if (pred(item)) {
				++result;
				++current;
				erase_after(prev);
				dispose(&item);
			} else {
				prev = current++;
			}
		}

		return result;
	}

	const_reference front() const noexcept {
		return *Cast(head.next);
	}

	reference front() noexcept {
		return *Cast(head.next);
	}

	reference pop_front() noexcept {
		auto &i = front();
		head.next = head.next->next;

		if constexpr (options.cache_last)
			if (head.next == nullptr)
				last_cache.value = &head;

		--counter;
		return i;
	}

	void pop_front_and_dispose(Disposer<value_type> auto disposer) noexcept {
		auto &i = pop_front();
		disposer(&i);
	}

	const_reference back() const noexcept
		requires(options.cache_last) {
		return *Cast(last_cache.value);
	}

	reference back() noexcept
		requires(options.cache_last) {
		return *Cast(last_cache.value);
	}

	class const_iterator;

	class iterator final {
		friend IntrusiveForwardList;
		friend const_iterator;

		IntrusiveForwardListNode *cursor;

		constexpr iterator(IntrusiveForwardListNode *_cursor) noexcept
			:cursor(_cursor) {}

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type *;
		using reference = value_type &;

		iterator() = default;

		constexpr bool operator==(const iterator &other) const noexcept {
			return cursor == other.cursor;
		}

		constexpr bool operator!=(const iterator &other) const noexcept {
			return !(*this == other);
		}

		constexpr reference operator*() const noexcept {
			return *Cast(cursor);
		}

		constexpr pointer operator->() const noexcept {
			return Cast(cursor);
		}

		iterator &operator++() noexcept {
			cursor = cursor->next;
			return *this;
		}

		iterator operator++(int) noexcept {
			auto old = *this;
			cursor = cursor->next;
			return old;
		}
	};

	constexpr iterator before_begin() noexcept {
		return {&head};
	}

	constexpr iterator begin() noexcept {
		return {head.next};
	}

	constexpr iterator end() noexcept {
		return {nullptr};
	}

	constexpr iterator last() noexcept
		requires(options.cache_last) {
		return {last_cache.value};
	}

	static constexpr iterator iterator_to(reference t) noexcept {
		return {&ToNode(t)};
	}

	class const_iterator final {
		friend IntrusiveForwardList;

		const IntrusiveForwardListNode *cursor;

		constexpr const_iterator(const IntrusiveForwardListNode *_cursor) noexcept
			:cursor(_cursor) {}

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = const T;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type *;
		using reference = value_type &;

		const_iterator() = default;

		const_iterator(iterator src) noexcept
			:cursor(src.cursor) {}

		constexpr bool operator==(const const_iterator &other) const noexcept {
			return cursor == other.cursor;
		}

		constexpr bool operator!=(const const_iterator &other) const noexcept {
			return !(*this == other);
		}

		constexpr reference operator*() const noexcept {
			return *Cast(cursor);
		}

		constexpr pointer operator->() const noexcept {
			return Cast(cursor);
		}

		const_iterator &operator++() noexcept {
			cursor = cursor->next;
			return *this;
		}

		const_iterator operator++(int) noexcept {
			auto old = *this;
			cursor = cursor->next;
			return old;
		}
	};

	constexpr const_iterator begin() const noexcept {
		return {head.next};
	}

	constexpr const_iterator end() const noexcept {
		return {nullptr};
	}

	constexpr const_iterator last() const noexcept
		requires(options.cache_last) {
		return {last_cache.value};
	}

	static constexpr const_iterator iterator_to(const_reference t) noexcept {
		return {&ToNode(t)};
	}

	iterator push_front(reference t) noexcept {
		auto &new_node = ToNode(t);

		if constexpr (options.cache_last)
			if (empty())
				last_cache.value = &new_node;

		new_node.next = head.next;
		head.next = &new_node;
		++counter;

		return iterator_to(t);
	}

	iterator push_back(reference t) noexcept
		requires(options.cache_last) {
		auto &new_node = ToNode(t);
		new_node.next = nullptr;
		last_cache.value->next = &new_node;
		last_cache.value = &new_node;

		++counter;

		return iterator_to(t);
	}

	static iterator insert_after(iterator pos, reference t) noexcept
		requires(!constant_time_size && !options.cache_last) {
		/* if we have no counter, then this method is allowed
		   to be static */

		auto &pos_node = *pos.cursor;
		auto &new_node = ToNode(t);
		new_node.next = pos_node.next;
		pos_node.next = &new_node;
		return &new_node;
	}

	iterator insert_after(iterator pos, reference t) noexcept
		requires(constant_time_size || options.cache_last) {
		auto &pos_node = *pos.cursor;
		auto &new_node = ToNode(t);

		if constexpr (options.cache_last)
			if (pos_node.next == nullptr)
				last_cache.value = &new_node;

		new_node.next = pos_node.next;
		pos_node.next = &new_node;
		++counter;
		return &new_node;
	}

	void erase_after(iterator pos) noexcept {
		pos.cursor->next = pos.cursor->next->next;

		if constexpr (options.cache_last)
			if (pos.cursor->next == nullptr)
				last_cache.value = pos.cursor;

		--counter;
	}

	void reverse() noexcept {
		if (empty())
			return;

		/* the first item will be the last, and will stay
		   there; during the loop, it will divide the list
		   between "old order" (right of it) and "new
		   (reversed) order" (left of it) */
		const auto middle = begin();

		while (std::next(middle) != end()) {
			/* remove the item after the "middle", and
			   move it to the front */
			auto i = std::next(middle);
			erase_after(middle);
			push_front(*i);
		}
	}
};
