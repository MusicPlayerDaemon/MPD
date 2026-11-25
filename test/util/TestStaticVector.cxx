// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/StaticVector.hxx"

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

TEST(StaticVector, CopyAndMoveSemanticsTrivial)
{
	using Vector = StaticVector<int, 4>;

	EXPECT_TRUE(std::is_trivially_copy_constructible_v<Vector>);
	EXPECT_TRUE(std::is_nothrow_copy_constructible_v<Vector>);
	EXPECT_TRUE(std::is_trivially_copy_assignable_v<Vector>);
	EXPECT_TRUE(std::is_nothrow_copy_assignable_v<Vector>);
	EXPECT_TRUE(std::is_trivially_move_constructible_v<Vector>);
	EXPECT_TRUE(std::is_nothrow_move_constructible_v<Vector>);
	EXPECT_TRUE(std::is_trivially_move_assignable_v<Vector>);
	EXPECT_TRUE(std::is_nothrow_move_assignable_v<Vector>);

	Vector source;
	source.emplace_back(1);
	source.emplace_back(2);

	auto copied = source;
	ASSERT_EQ(copied.size(), 2u);
	EXPECT_EQ(copied[0], 1);
	EXPECT_EQ(copied[1], 2);

	Vector copy_assigned;
	copy_assigned.emplace_back(9);
	copy_assigned = source;
	ASSERT_EQ(copy_assigned.size(), 2u);
	EXPECT_EQ(copy_assigned[0], 1);
	EXPECT_EQ(copy_assigned[1], 2);

	auto moved = std::move(copied);
	ASSERT_EQ(moved.size(), 2u);
	EXPECT_EQ(moved[0], 1);
	EXPECT_EQ(moved[1], 2);

	Vector move_assigned;
	move_assigned.emplace_back(7);
	move_assigned = std::move(copy_assigned);
	ASSERT_EQ(move_assigned.size(), 2u);
	EXPECT_EQ(move_assigned[0], 1);
	EXPECT_EQ(move_assigned[1], 2);
}

class CopyMoveValue {
	std::unique_ptr<int> value;

public:
	explicit CopyMoveValue(int src)
		:value(std::make_unique<int>(src)) {}

	CopyMoveValue(const CopyMoveValue &src)
		:value(std::make_unique<int>(*src.value)) {}

	CopyMoveValue(CopyMoveValue &&) noexcept = default;

	CopyMoveValue &operator=(const CopyMoveValue &src)
	{
		if (this != &src)
			value = std::make_unique<int>(*src.value);

		return *this;
	}

	CopyMoveValue &operator=(CopyMoveValue &&) noexcept = default;

	int Get() const noexcept {
		return *value;
	}

	void Set(int src) noexcept {
		*value = src;
	}
};

TEST(StaticVector, CopyAndMoveSemantics)
{
	using Vector = StaticVector<CopyMoveValue, 4>;

	EXPECT_FALSE(std::is_trivially_copy_constructible_v<Vector>);
	EXPECT_FALSE(std::is_nothrow_copy_constructible_v<Vector>);
	EXPECT_FALSE(std::is_trivially_copy_assignable_v<Vector>);
	EXPECT_FALSE(std::is_nothrow_copy_assignable_v<Vector>);
	EXPECT_FALSE(std::is_trivially_move_constructible_v<Vector>);
	EXPECT_TRUE(std::is_nothrow_move_constructible_v<Vector>);
	EXPECT_FALSE(std::is_trivially_move_assignable_v<Vector>);
	EXPECT_TRUE(std::is_nothrow_move_assignable_v<Vector>);

	Vector source;
	source.emplace_back(1);
	source.emplace_back(2);

	auto copied = source;
	ASSERT_EQ(copied.size(), 2u);
	EXPECT_EQ(copied[0].Get(), 1);
	EXPECT_EQ(copied[1].Get(), 2);

	copied[0].Set(10);
	EXPECT_EQ(source[0].Get(), 1);

	Vector copy_assigned;
	copy_assigned.emplace_back(9);
	copy_assigned = source;
	ASSERT_EQ(copy_assigned.size(), 2u);
	EXPECT_EQ(copy_assigned[0].Get(), 1);
	EXPECT_EQ(copy_assigned[1].Get(), 2);

	copy_assigned[1].Set(20);
	EXPECT_EQ(source[1].Get(), 2);

	auto moved = std::move(copied);
	ASSERT_EQ(moved.size(), 2u);
	EXPECT_EQ(moved[0].Get(), 10);
	EXPECT_EQ(moved[1].Get(), 2);

	Vector move_assigned;
	move_assigned.emplace_back(7);
	move_assigned = std::move(copy_assigned);
	ASSERT_EQ(move_assigned.size(), 2u);
	EXPECT_EQ(move_assigned[0].Get(), 1);
	EXPECT_EQ(move_assigned[1].Get(), 20);
}
