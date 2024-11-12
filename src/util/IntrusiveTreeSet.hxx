// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "RedBlackTree.hxx"

#include "Cast.hxx"
#include "Concepts.hxx"
#include "IntrusiveHookMode.hxx" // IWYU pragma: export
#include "MemberPointer.hxx"
#include "OptionalCounter.hxx"

#include <cassert>
#include <compare> // for std::weak_ordering
#include <concepts> // for std::regular_invocable
#include <optional>
#include <utility> // for std::exchange()

struct IntrusiveTreeSetOptions {
	bool constant_time_size = false;
};

template<IntrusiveHookMode _mode=IntrusiveHookMode::NORMAL>
class IntrusiveTreeSetHook {
	template<typename T> friend struct IntrusiveTreeSetBaseHookTraits;
	template<auto member> friend struct IntrusiveTreeSetMemberHookTraits;
	template<typename T, typename Operators, typename HookTraits, IntrusiveTreeSetOptions> friend class IntrusiveTreeSet;

protected:
	RedBlackTreeNode node;

public:
	static constexpr IntrusiveHookMode mode = _mode;

	constexpr IntrusiveTreeSetHook() noexcept {
		if constexpr (mode >= IntrusiveHookMode::TRACK)
			node.parent = nullptr;
	}

	constexpr ~IntrusiveTreeSetHook() noexcept {
		if constexpr (mode >= IntrusiveHookMode::AUTO_UNLINK)
			if (is_linked())
				unlink();
	}

	IntrusiveTreeSetHook(const IntrusiveTreeSetHook &) = delete;
	IntrusiveTreeSetHook &operator=(const IntrusiveTreeSetHook &) = delete;

	constexpr void unlink() noexcept {
		if constexpr (mode >= IntrusiveHookMode::TRACK) {
			assert(is_linked());
		}

		node.Unlink();

		if constexpr (mode >= IntrusiveHookMode::TRACK)
			node.parent = nullptr;
	}

	bool is_linked() const noexcept {
		static_assert(mode >= IntrusiveHookMode::TRACK);

		return node.parent != nullptr;
	}

private:
	static constexpr auto &Cast(RedBlackTreeNode &node) noexcept {
		return ContainerCast(node, &IntrusiveTreeSetHook::node);
	}

	static constexpr const auto &Cast(const RedBlackTreeNode &node) noexcept {
		return ContainerCast(node, &IntrusiveTreeSetHook::node);
	}
};

/**
 * Detect the hook type.
 */
template<typename U>
struct IntrusiveTreeSetHookDetection {
	/* TODO can this be simplified somehow, without checking for
	   all possible enum values? */
	using type = std::conditional_t<std::is_base_of_v<IntrusiveTreeSetHook<IntrusiveHookMode::NORMAL>, U>,
					IntrusiveTreeSetHook<IntrusiveHookMode::NORMAL>,
					std::conditional_t<std::is_base_of_v<IntrusiveTreeSetHook<IntrusiveHookMode::TRACK>, U>,
							   IntrusiveTreeSetHook<IntrusiveHookMode::TRACK>,
							   std::conditional_t<std::is_base_of_v<IntrusiveTreeSetHook<IntrusiveHookMode::AUTO_UNLINK>, U>,
									      IntrusiveTreeSetHook<IntrusiveHookMode::AUTO_UNLINK>,
									      void>>>;
};

/**
 * For classes which embed #IntrusiveTreeSetHook as base class.
 */
template<typename T>
struct IntrusiveTreeSetBaseHookTraits {
	template<typename U>
	using Hook = typename IntrusiveTreeSetHookDetection<U>::type;

	static constexpr T *Cast(RedBlackTreeNode *node) noexcept {
		auto *hook = &Hook<T>::Cast(*node);
		return static_cast<T *>(hook);
	}

	static constexpr auto &ToHook(T &t) noexcept {
		return static_cast<Hook<T> &>(t);
	}
};

/**
 * For classes which embed #IntrusiveTreeSetHook as member.
 */
template<auto member>
struct IntrusiveTreeSetMemberHookTraits {
	using T = MemberPointerContainerType<decltype(member)>;
	using _Hook = MemberPointerType<decltype(member)>;

	template<typename Dummy>
	using Hook = _Hook;

	static constexpr T *Cast(RedBlackTreeNode *node) noexcept {
		auto &hook = Hook<T>::Cast(*node);
		return &ContainerCast(hook, member);
	}

	static constexpr auto &ToHook(T &t) noexcept {
		return t.*member;
	}
};

/**
 * @param GetKey a function object which extracts the "key" part of an
 * item
 */
template<typename T,
	 std::regular_invocable<const T &> GetKey=std::identity,
	 std::regular_invocable<std::invoke_result_t<GetKey, const T &>,
				std::invoke_result_t<GetKey, const T &>> Compare=std::compare_three_way>
struct IntrusiveTreeSetOperators {
	[[no_unique_address]]
	GetKey get_key;

	[[no_unique_address]]
	Compare compare;
};

/**
 * A binary tree implementation which stores pointers to items which
 * have an embedded #IntrusiveTreeSetHook.
 */
template<typename T,
	 typename Operators=IntrusiveTreeSetOperators<T>,
	 typename HookTraits=IntrusiveTreeSetBaseHookTraits<T>,
	 IntrusiveTreeSetOptions options=IntrusiveTreeSetOptions{}>
class IntrusiveTreeSet {
	static constexpr bool constant_time_size = options.constant_time_size;

	[[no_unique_address]]
	OptionalCounter<constant_time_size> counter;

	[[no_unique_address]]
	Operators ops;

	RedBlackTreeNode head{RedBlackTreeNode::Head{}};

public:
	using value_type = T;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	using size_type = std::size_t;

	[[nodiscard]]
	IntrusiveTreeSet() noexcept = default;

#ifndef NDEBUG
	/**
	 * For debugging only: check the integrity of the red-black
	 * tree.
	 */
	void Check() noexcept {
		RedBlackTreeNode::BlackHeight(GetRoot());
	}
#endif

	[[nodiscard]]
	constexpr bool empty() const noexcept {
		return GetRoot() == nullptr;
	}

	[[nodiscard]]
	constexpr size_type size() const noexcept {
		if constexpr (constant_time_size)
			return counter;
		else
			return std::distance(begin(), end());
	}

	constexpr void clear() noexcept {
		SetRoot(nullptr);
		counter.reset();
	}

	constexpr void clear_and_dispose(Disposer<value_type> auto disposer) noexcept {
		dispose_all(GetRoot(), disposer);
		clear();
	}

	class iterator {
		friend IntrusiveTreeSet;

		RedBlackTreeNode *node;

	public:
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type *;
		using reference = value_type &;

		explicit constexpr iterator(RedBlackTreeNode *_node) noexcept
			:node(_node) {}

		constexpr bool operator==(const iterator &) const noexcept = default;
		constexpr bool operator!=(const iterator &) const noexcept = default;

		constexpr reference operator*() const noexcept {
			return *Cast(node);
		}

		constexpr pointer operator->() const noexcept {
			return Cast(node);
		}

		constexpr auto &operator++() noexcept {
			node = RedBlackTreeNode::GetNextNode(node);
			return *this;
		}
	};

	[[nodiscard]]
	constexpr iterator begin() noexcept {
		auto *root = GetRoot();
		return root != nullptr
			? iterator{&root->GetLeftMost()}
			: end();
	}

	[[nodiscard]]
	constexpr iterator end() noexcept {
		return iterator{nullptr};
	}

	class const_iterator {
		friend IntrusiveTreeSet;

		const RedBlackTreeNode *node;

	public:
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = const T;
		using difference_type = std::ptrdiff_t;
		using pointer = value_type *;
		using reference = value_type &;

		explicit constexpr const_iterator(RedBlackTreeNode *_node) noexcept
			:node(_node) {}

		constexpr const_iterator(iterator i) noexcept
			:node(i.node) {}

		constexpr bool operator==(const const_iterator &) const noexcept = default;
		constexpr bool operator!=(const const_iterator &) const noexcept = default;

		constexpr reference operator*() const noexcept {
			return *Cast(node);
		}

		constexpr pointer operator->() const noexcept {
			return Cast(node);
		}

		constexpr auto &operator++() noexcept {
			node = RedBlackTreeNode::GetNextNode(const_cast<RedBlackTreeNode *>(node));
			return *this;
		}
	};

	[[nodiscard]]
	constexpr const_iterator begin() const noexcept {
		auto *root = GetRoot();
		return root != nullptr
			? const_iterator{&root->GetLeftMost()}
			: end();
	}

	[[nodiscard]]
	constexpr const_iterator end() const noexcept {
		return const_iterator{nullptr};
	}

	const_reference front() const noexcept {
		auto i = begin();
		assert(i != end());

		return *i;
	}

	reference front() noexcept {
		auto i = begin();
		assert(i != end());

		return *i;
	}

	[[nodiscard]]
	static constexpr iterator iterator_to(reference item) noexcept {
		return iterator{&ToNode(item)};
	}

	[[nodiscard]]
	constexpr iterator find(const auto &key) const noexcept {
		auto *node = GetRoot();

#ifndef NDEBUG
		bool previous_red = false;
#endif

		while (node != nullptr) {
#ifndef NDEBUG
			const bool current_red = node->color == RedBlackTreeNode::Color::RED;
			assert(!previous_red || !current_red);
			previous_red = current_red;
#endif

			const auto &item = *Cast(node);

			const std::weak_ordering compare_result = ops.compare(key, ops.get_key(item));
			if (compare_result == std::weak_ordering::less)
				node = node->GetLeft();
			else if (compare_result == std::weak_ordering::greater)
				node = node->GetRight();
			else
				break;
		}

		return iterator{node};
	}

	constexpr iterator insert(reference value) noexcept {
		static_assert(!constant_time_size ||
			      GetHookMode() < IntrusiveHookMode::AUTO_UNLINK,
			      "Can't use auto-unlink hooks with constant_time_size");

		auto *root = GetRoot();
		if (root == nullptr) {
			root = &ToNode(value);
			root->Init(RedBlackTreeNode::Color::BLACK);
		} else {
			root = &insert(root, value);
		}

		SetRoot(root);

		++counter;

		return iterator_to(value);
	}

	iterator erase(iterator i) noexcept {
		assert(i.node != nullptr);
		assert(!empty());

		auto *next = RedBlackTreeNode::GetNextNode(i.node);
		Cast(i.node)->unlink();
		--counter;
		return iterator{next};
	}

	void pop_front() noexcept {
		erase(begin());
	}

private:
	[[nodiscard]]
	static constexpr auto GetHookMode() noexcept {
		return HookTraits::template Hook<T>::mode;
	}

	[[nodiscard]]
	static constexpr pointer Cast(RedBlackTreeNode *node) noexcept {
		return HookTraits::Cast(node);
	}

	[[nodiscard]]
	static constexpr const_pointer Cast(const RedBlackTreeNode *node) noexcept {
		return HookTraits::Cast(const_cast<RedBlackTreeNode *>(node));
	}

	[[nodiscard]]
	static constexpr auto &ToHook(T &t) noexcept {
		return HookTraits::ToHook(t);
	}

	[[nodiscard]]
	static constexpr const auto &ToHook(const T &t) noexcept {
		return HookTraits::ToHook(t);
	}

	[[nodiscard]]
	static constexpr RedBlackTreeNode &ToNode(T &t) noexcept {
		return ToHook(t).node;
	}

	[[nodiscard]]
	static constexpr const RedBlackTreeNode &ToNode(const T &t) noexcept {
		return ToHook(t).node;
	}

	[[nodiscard]]
	constexpr RedBlackTreeNode *GetRoot() const noexcept {
		return head.GetLeft();
	}

	[[nodiscard]]
	constexpr bool IsRoot(const RedBlackTreeNode &node) const noexcept {
		return &node == GetRoot();
	}

	constexpr void SetRoot(RedBlackTreeNode *root) noexcept {
		head.SetChild(RedBlackTreeNode::Direction::LEFT, root);
	}

	[[gnu::pure]]
	constexpr RedBlackTreeNode::Direction GetInsertDirection(RedBlackTreeNode &parent,
								 const_reference new_value) const noexcept {
		const auto &parent_value = *Cast(&parent);
		const std::weak_ordering compare_result = ops.compare(ops.get_key(new_value), ops.get_key(parent_value));
		return compare_result == std::weak_ordering::less
			? RedBlackTreeNode::Direction::LEFT
			: RedBlackTreeNode::Direction::RIGHT;
	}

	std::optional<RedBlackTreeNode::Direction> rotate1, rotate2;

	RedBlackTreeNode &insert(RedBlackTreeNode *base,
				 reference value) noexcept {
		if (base == nullptr) {
			auto &node = ToNode(value);
			node.Init(RedBlackTreeNode::Color::RED);
			return node;
		}

		/* the actual insert is here */
		const auto insert_direction = GetInsertDirection(*base, value);
		auto &new_child = insert(base->GetChild(insert_direction), value);
		base->SetChild(insert_direction, &new_child);
		const bool red_red_conflict = !IsRoot(*base) &&
			base->color == RedBlackTreeNode::Color::RED &&
			new_child.color == RedBlackTreeNode::Color::RED;

		/* rotate */
		if (rotate1) {
			base->SetChild(*rotate1, &base->GetChild(*rotate1)->Rotate(*rotate1));
			rotate1.reset();
		}

		if (rotate2) {
			base->color = RedBlackTreeNode::Color::RED;
			base = &base->Rotate(*rotate2);
			base->color = RedBlackTreeNode::Color::BLACK;
			rotate2.reset();
		}

		if (red_red_conflict) {
			const auto direction = base->GetDirectionInParent();
			const auto other_direction = RedBlackTreeNode::OtherDirection(direction);

			if (auto *sibling = base->parent->GetChild(other_direction);
			    sibling != nullptr &&
			    sibling->color == RedBlackTreeNode::Color::RED) {
				sibling->color = RedBlackTreeNode::Color::BLACK;
				base->color = RedBlackTreeNode::Color::BLACK;
				if (!IsRoot(*base->parent))
					base->parent->color = RedBlackTreeNode::Color::RED;
			} else if (const auto *other_child = base->GetChild(other_direction);
				   other_child != nullptr && other_child->color == RedBlackTreeNode::Color::RED) {
				rotate1 = direction;
				rotate2 = other_direction;
			} else if (const auto *child = base->GetChild(direction);
				   child != nullptr && child->color == RedBlackTreeNode::Color::RED) {
				rotate2 = other_direction;
			}
		}

		return *base;
	}

	void dispose_all(RedBlackTreeNode *node, Disposer<value_type> auto disposer) noexcept {
		if (node == nullptr)
			return;

		for (auto *i : node->children)
			dispose_all(i, disposer);

		disposer(Cast(node));
	}
};
