// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <algorithm> // for std::any_of()
#include <array>
#include <cassert>
#include <utility> // for std::exchange()

struct RedBlackTreeNode {
	RedBlackTreeNode *parent;

	enum class Direction : std::size_t { LEFT, RIGHT };

	std::array<RedBlackTreeNode *, 2> children;

	enum class Color { HEAD, BLACK, RED };

	Color color;

	constexpr RedBlackTreeNode() noexcept = default;

	struct Head {};
	explicit constexpr RedBlackTreeNode(Head) noexcept
		:children({}),
		 color(Color::HEAD) {}

	RedBlackTreeNode(const RedBlackTreeNode &) = delete;
	RedBlackTreeNode &operator=(const RedBlackTreeNode &) = delete;

	constexpr void Init(Color _color) noexcept {
		children = {};
		color = _color;
	}

	[[nodiscard]]
	constexpr bool IsHead() const noexcept {
		return color == Color::HEAD;
	}

	[[nodiscard]]
	constexpr bool IsRoot() const noexcept {
		assert(!IsHead());

		return parent->IsHead();
	}

	[[nodiscard]]
	static constexpr Direction OtherDirection(Direction direction) noexcept {
		return static_cast<Direction>(static_cast<std::size_t>(direction) ^ 1);
	}

	[[nodiscard]]
	constexpr RedBlackTreeNode *GetChild(Direction direction) const noexcept {
		return children[static_cast<std::size_t>(direction)];
	}

	[[nodiscard]]
	constexpr RedBlackTreeNode *GetLeft() const noexcept {
		return GetChild(Direction::LEFT);
	}

	[[nodiscard]]
	constexpr RedBlackTreeNode *GetRight() const noexcept {
		return GetChild(Direction::RIGHT);
	}

	[[nodiscard]]
	constexpr RedBlackTreeNode *GetOtherChild(Direction direction) const noexcept {
		return GetChild(OtherDirection(direction));
	}

	/**
	 * Set a new child and return the old one.
	 */
	constexpr auto *SetChild(Direction direction,
				 RedBlackTreeNode *child) noexcept {
		auto *old = std::exchange(children[static_cast<std::size_t>(direction)],
					  child);
		if (child != nullptr)
			child->parent = this;

		return old;
	}

	constexpr auto *SetChild(Direction direction,
				 RedBlackTreeNode &child) noexcept {
		auto *old = std::exchange(children[static_cast<std::size_t>(direction)],
					  &child);
		child.parent = this;

		return old;
	}

	constexpr auto *SetOtherChild(Direction direction,
				      RedBlackTreeNode *child) noexcept {
		return SetChild(OtherDirection(direction), child);
	}

	[[nodiscard]]
	constexpr Direction GetChildDirection(const RedBlackTreeNode &child) const noexcept {
		assert(child.parent == this);
		assert(&child == GetChild(Direction::LEFT) ||
		       &child == GetChild(Direction::RIGHT));

		return &child == GetChild(Direction::LEFT)
			? Direction::LEFT
			: Direction::RIGHT;
	}

	constexpr void ReplaceChild(RedBlackTreeNode &old_child,
				    RedBlackTreeNode *new_child) noexcept {
		SetChild(GetChildDirection(old_child), new_child);
	}

	constexpr void ReplaceChild(RedBlackTreeNode &old_child,
				    RedBlackTreeNode &new_child) noexcept {
		SetChild(GetChildDirection(old_child), new_child);
	}

	[[nodiscard]]
	constexpr Direction GetDirectionInParent() const noexcept {
		assert(parent != nullptr);
		assert(!IsHead());

		return parent->GetChildDirection(*this);
	}

	[[nodiscard]]
	auto &Rotate(RedBlackTreeNode::Direction direction) noexcept {
		assert(!IsHead());

		auto *x = GetOtherChild(direction);
		assert(x != nullptr);

		auto *y = x->SetChild(direction, this);
		SetOtherChild(direction, y);

		return *x;
	}

	void RotateInParent(RedBlackTreeNode::Direction direction) noexcept {
		assert(parent != nullptr);
		assert(!IsHead());

		auto &p = *parent;
		const auto direction_in_parent = p.GetChildDirection(*this);

		auto &new_node = Rotate(direction);
		assert(new_node.parent == this);

		assert(p.GetChild(direction_in_parent) == this);
		p.SetChild(direction_in_parent, new_node);
	}

	[[nodiscard]]
	constexpr static RedBlackTreeNode &GetLeftMost(RedBlackTreeNode *node) noexcept {
		assert(node != nullptr);
		assert(!node->IsHead());

		while (auto *left = node->GetChild(Direction::LEFT)) {
			assert(left->parent == node);
			node = left;
		}

		return *node;
	}

	[[nodiscard]]
	constexpr RedBlackTreeNode &GetLeftMost() noexcept {
		return GetLeftMost(this);
	}

private:
	[[nodiscard]]
	constexpr static RedBlackTreeNode *GetLeftHandedParent(RedBlackTreeNode *node) noexcept {
		assert(node != nullptr);
		assert(!node->IsHead());

		while (true) {
			assert(node->parent != nullptr);
			auto &p = *node->parent;
			if (p.IsHead())
				return nullptr;

			assert(node->color != RedBlackTreeNode::Color::RED ||
			       p.color != RedBlackTreeNode::Color::RED);

			if (p.GetChildDirection(*node) == Direction::LEFT)
				return &p;

			node = &p;
		}
	}

public:
	[[nodiscard]]
	constexpr static RedBlackTreeNode *GetNextNode(RedBlackTreeNode *node) noexcept {
		assert(node != nullptr);
		assert(!node->IsHead());

		if (auto *right = node->GetChild(Direction::RIGHT)) {
			assert(node->color != RedBlackTreeNode::Color::RED ||
			       right->color != RedBlackTreeNode::Color::RED);
			return &right->GetLeftMost();
		}

		assert(node->parent != nullptr);
		auto &p = *node->parent;
		if (p.IsHead())
			return nullptr;

		if (p.GetChildDirection(*node) == Direction::LEFT)
			return &p;

		return GetLeftHandedParent(&p);
	}

private:
	[[nodiscard]]
	constexpr bool HasTwoChildren() const noexcept {
		return children[0] != nullptr && children[1] != nullptr;
	}

	constexpr RedBlackTreeNode *GetAnyChild() const noexcept {
		return children[children[1] != nullptr];
	}

public:
	constexpr void Unlink() noexcept {
		assert(parent != nullptr);
		assert(!IsHead());

		if (HasTwoChildren()) {
			/* swap with successor, because it, by
			   definition, doesn't have two children; the
			   rest of this method assumes we have exactly
			   one child or none */

			auto &right = *GetRight();
			auto &successor = right.GetLeftMost();

			auto &p = *parent;
			const auto direction_in_parent = p.GetChildDirection(*this);

			successor.SetChild(Direction::LEFT, GetLeft());
			SetChild(Direction::LEFT, nullptr);
			SetChild(Direction::RIGHT, successor.GetRight());

			if (&successor == &right) {
				assert(successor.parent == this);

				successor.SetChild(Direction::RIGHT, *this);
			} else {
				assert(successor.parent != this);

				successor.parent->SetChild(Direction::LEFT, *this);
			}

			p.SetChild(direction_in_parent, successor);
		} else {
			/* if there is exactly one child, it must be red */
			assert(GetAnyChild() == nullptr || GetAnyChild()->color == Color::RED);
		}

		assert(!HasTwoChildren());

		auto &p = *parent;

		if (auto *child = GetAnyChild()) {
			p.ReplaceChild(*this, *child);
			child->color = Color::BLACK;
		} else if (IsRoot()) {
			p.SetChild(Direction::LEFT, nullptr);
		} else {
			if (color == Color::BLACK)
				FixDoubleBlack();

			p.ReplaceChild(*this, nullptr);
		}
	}

private:
	constexpr std::pair<Direction, RedBlackTreeNode *> GetRedChild() const noexcept {
		if (auto *left = GetLeft(); left != nullptr && left->color == Color::RED)
			return {Direction::LEFT, left};

		if (auto *right = GetRight(); right != nullptr && right->color == Color::RED)
			return {Direction::RIGHT, right};

		return {};
	}

	constexpr void FixDoubleBlack() noexcept {
		assert(parent != nullptr);
		assert(!IsHead());
		assert(color == Color::BLACK);

		if (IsRoot())
			return;

		auto &p = *parent;
		const auto direction = p.GetChildDirection(*this);
		const auto other_direction = OtherDirection(direction);
		auto *const sibling = p.GetChild(other_direction);

		if (sibling == nullptr) {
			p.FixDoubleBlack();
			return;
		}

		switch (sibling->color) {
		case Color::RED:
			p.color = Color::RED;
			sibling->color = Color::BLACK;

			p.RotateInParent(direction);
			FixDoubleBlack();
			break;

		case Color::BLACK:
			if (const auto [red_direction, red] = sibling->GetRedChild(); red != nullptr) {
				/* at least one red child */

				if (direction == red_direction) {
					red->color = p.color;
					sibling->RotateInParent(other_direction);
				} else {
					red->color = sibling->color;
					sibling->color = p.color;
				}

				p.RotateInParent(direction);
				p.color = Color::BLACK;
			} else {
				/* no red child (both children are
				   either black or nullptr) */

				sibling->color = Color::RED;
				if (p.color == Color::BLACK)
					p.FixDoubleBlack();
				else
					p.color = Color::BLACK;
			}

			break;

		case Color::HEAD:
			// unreachable
			assert(false);
			break;
		}
	}
};
