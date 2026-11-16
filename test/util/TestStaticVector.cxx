// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/StaticVector.hxx"

#include <gtest/gtest.h>

#include <iterator>
#include <memory>
#include <type_traits>

namespace {

/**
 * A type that counts how many instances are currently alive, to verify
 * that destructors are invoked the correct number of times.
 */
struct Counted {
	static inline int live = 0;

	int value;

	explicit Counted(int _value) noexcept:value(_value) { ++live; }
	Counted(const Counted &src) noexcept:value(src.value) { ++live; }
	Counted(Counted &&src) noexcept:value(src.value) { ++live; }
	~Counted() noexcept { --live; }

	/* assignment reuses an existing instance, so it does not
	   change the live count */
	Counted &operator=(const Counted &) noexcept = default;
	Counted &operator=(Counted &&) noexcept = default;
};

} // anonymous namespace

/**
 * Regression test: erase() must destroy the slots vacated at the end
 * after moving the tail down.
 */
TEST(StaticVector, EraseDestroys)
{
	ASSERT_EQ(Counted::live, 0);

	{
		StaticVector<Counted, 8> v;
		for (int i = 0; i < 5; ++i)
			v.emplace_back(i);
		ASSERT_EQ(v.size(), 5u);
		ASSERT_EQ(Counted::live, 5);

		/* erase a single element in the middle */
		v.erase(std::next(v.begin(), 1));
		EXPECT_EQ(v.size(), 4u);
		EXPECT_EQ(Counted::live, 4);

		/* erase a range of two elements */
		v.erase(std::next(v.begin(), 1), std::next(v.begin(), 3));
		EXPECT_EQ(v.size(), 2u);
		EXPECT_EQ(Counted::live, 2);

		/* the survivors are the expected ones, in order */
		EXPECT_EQ(v[0].value, 0);
		EXPECT_EQ(v[1].value, 4);
	}

	/* nothing leaked after the vector was destroyed */
	EXPECT_EQ(Counted::live, 0);
}

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
