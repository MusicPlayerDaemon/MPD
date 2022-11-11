/*
 * Copyright 2020-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Cast.hxx"
#include "MemberPointer.hxx"
#include "OptionalCounter.hxx"
#include "ShallowCopy.hxx"

#include <iterator>
#include <type_traits>
#include <utility>

struct IntrusiveForwardListNode {
	IntrusiveForwardListNode *next = nullptr;
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
	 bool constant_time_size=false>
class IntrusiveForwardList {
	IntrusiveForwardListNode head;

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
		swap(counter, src.counter);
	}

	constexpr IntrusiveForwardList(ShallowCopy, const IntrusiveForwardList &src) noexcept
		:head(src.head)
	{
		// shallow copies mess with the counter
		static_assert(!constant_time_size);
	}

	IntrusiveForwardList &operator=(IntrusiveForwardList &&src) noexcept {
		using std::swap;
		swap(head, src.head);
		swap(counter, counter);
		return *this;
	}

	constexpr bool empty() const noexcept {
		return head.next == nullptr;
	}

	constexpr size_type size() const noexcept {
		if constexpr (constant_time_size)
			return counter;
		else
			return std::distance(begin(), end());
	}

	void clear() noexcept {
		head = {};
		counter.reset();
	}

	template<typename D>
	void clear_and_dispose(D &&disposer) noexcept {
		while (!empty()) {
			auto *item = &front();
			pop_front();
			disposer(item);
		}
	}

	const_reference front() const noexcept {
		return *Cast(head.next);
	}

	reference front() noexcept {
		return *Cast(head.next);
	}

	void pop_front() noexcept {
		head.next = head.next->next;
		--counter;
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
	};

	constexpr iterator before_begin() noexcept {
		return {&head};
	}

	constexpr iterator begin() noexcept {
		return {head.next};
	}

	static constexpr iterator end() noexcept {
		return {nullptr};
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
	};

	constexpr const_iterator begin() const noexcept {
		return {head.next};
	}

	void push_front(reference t) noexcept {
		auto &new_node = ToNode(t);
		new_node.next = head.next;
		head.next = &new_node;
		++counter;
	}

	static iterator insert_after(iterator pos, reference t) noexcept {
		// no counter update in this static method
		static_assert(!constant_time_size);

		auto &pos_node = *pos.cursor;
		auto &new_node = ToNode(t);
		new_node.next = pos_node.next;
		pos_node.next = &new_node;
		return &new_node;
	}

	void erase_after(iterator pos) noexcept {
		pos.cursor->next = pos.cursor->next->next;
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
