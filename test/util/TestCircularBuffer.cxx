/*
 * Unit tests for class CircularBuffer.
 */

#include "util/CircularBuffer.hxx"

#include <gtest/gtest.h>

TEST(CircularBuffer, Basic)
{
	constexpr size_t N = 8;
	int data[N];
	CircularBuffer<int> buffer(data, N);

	EXPECT_EQ(size_t(N), buffer.GetCapacity());

	/* '.' = empty; 'O' = occupied; 'X' = blocked */

	/* checks on empty buffer */
	/* [.......X] */
	EXPECT_TRUE(buffer.empty());
	EXPECT_FALSE(buffer.IsFull());
	EXPECT_EQ(size_t(0), buffer.GetSize());
	EXPECT_EQ(size_t(7), buffer.GetSpace());
	EXPECT_TRUE(buffer.Read().empty());
	EXPECT_FALSE(buffer.Write().empty());
	EXPECT_EQ(&data[0], buffer.Write().data);
	EXPECT_EQ(size_t(7), buffer.Write().size);

	/* append one element */
	/* [O......X] */
	buffer.Append(1);
	EXPECT_FALSE(buffer.empty());
	EXPECT_FALSE(buffer.IsFull());
	EXPECT_FALSE(buffer.Read().empty());
	EXPECT_EQ(size_t(1), buffer.GetSize());
	EXPECT_EQ(size_t(6), buffer.GetSpace());
	EXPECT_EQ(size_t(1), buffer.Read().size);
	EXPECT_EQ(&data[0], buffer.Read().data);
	EXPECT_FALSE(buffer.Write().empty());
	EXPECT_EQ(&data[1], buffer.Write().data);
	EXPECT_EQ(size_t(6), buffer.Write().size);

	/* append 6 elements, buffer is now full */
	/* [OOOOOOOX] */
	buffer.Append(6);
	EXPECT_FALSE(buffer.empty());
	EXPECT_TRUE(buffer.IsFull());
	EXPECT_FALSE(buffer.Read().empty());
	EXPECT_EQ(size_t(7), buffer.GetSize());
	EXPECT_EQ(size_t(0), buffer.GetSpace());
	EXPECT_EQ(size_t(7), buffer.Read().size);
	EXPECT_EQ(&data[0], buffer.Read().data);
	EXPECT_TRUE(buffer.Write().empty());

	/* consume [0]; can append one at [7] */
	/* [XOOOOOO.] */
	buffer.Consume(1);
	EXPECT_FALSE(buffer.empty());
	EXPECT_FALSE(buffer.IsFull());
	EXPECT_FALSE(buffer.Read().empty());
	EXPECT_EQ(size_t(6), buffer.GetSize());
	EXPECT_EQ(size_t(1), buffer.GetSpace());
	EXPECT_EQ(size_t(6), buffer.Read().size);
	EXPECT_EQ(&data[1], buffer.Read().data);
	EXPECT_FALSE(buffer.Write().empty());
	EXPECT_EQ(&data[7], buffer.Write().data);
	EXPECT_EQ(size_t(1), buffer.Write().size);

	/* append one element; [0] is still empty but cannot
	   be written to because head==1 */
	/* [XOOOOOOO] */
	buffer.Append(1);
	EXPECT_FALSE(buffer.empty());
	EXPECT_TRUE(buffer.IsFull());
	EXPECT_FALSE(buffer.Read().empty());
	EXPECT_EQ(size_t(7), buffer.GetSize());
	EXPECT_EQ(size_t(0), buffer.GetSpace());
	EXPECT_EQ(size_t(7), buffer.Read().size);
	EXPECT_EQ(&data[1], buffer.Read().data);
	EXPECT_TRUE(buffer.Write().empty());

	/* consume [1..3]; can append [0..2] */
	/* [...XOOOO] */
	buffer.Consume(3);
	EXPECT_FALSE(buffer.empty());
	EXPECT_FALSE(buffer.IsFull());
	EXPECT_FALSE(buffer.Read().empty());
	EXPECT_EQ(size_t(4), buffer.GetSize());
	EXPECT_EQ(size_t(3), buffer.GetSpace());
	EXPECT_EQ(size_t(4), buffer.Read().size);
	EXPECT_EQ(&data[4], buffer.Read().data);
	EXPECT_FALSE(buffer.Write().empty());
	EXPECT_EQ(&data[0], buffer.Write().data);
	EXPECT_EQ(size_t(3), buffer.Write().size);

	/* append [0..1] */
	/* [OO.XOOOO] */
	buffer.Append(2);
	EXPECT_FALSE(buffer.empty());
	EXPECT_FALSE(buffer.IsFull());
	EXPECT_FALSE(buffer.Read().empty());
	EXPECT_EQ(size_t(6), buffer.GetSize());
	EXPECT_EQ(size_t(1), buffer.GetSpace());
	EXPECT_EQ(size_t(4), buffer.Read().size);
	EXPECT_EQ(&data[4], buffer.Read().data);
	EXPECT_FALSE(buffer.Write().empty());
	EXPECT_EQ(&data[2], buffer.Write().data);
	EXPECT_EQ(size_t(1), buffer.Write().size);

	/* append [2] */
	/* [OOOXOOOO] */
	buffer.Append(1);
	EXPECT_FALSE(buffer.empty());
	EXPECT_TRUE(buffer.IsFull());
	EXPECT_FALSE(buffer.Read().empty());
	EXPECT_EQ(size_t(7), buffer.GetSize());
	EXPECT_EQ(size_t(0), buffer.GetSpace());
	EXPECT_EQ(size_t(4), buffer.Read().size);
	EXPECT_EQ(&data[4], buffer.Read().data);
	EXPECT_TRUE(buffer.Write().empty());

	/* consume [4..7] */
	/* [OOO....X] */
	buffer.Consume(4);
	EXPECT_FALSE(buffer.empty());
	EXPECT_FALSE(buffer.IsFull());
	EXPECT_FALSE(buffer.Read().empty());
	EXPECT_EQ(size_t(3), buffer.GetSize());
	EXPECT_EQ(size_t(4), buffer.GetSpace());
	EXPECT_EQ(size_t(3), buffer.Read().size);
	EXPECT_EQ(&data[0], buffer.Read().data);
	EXPECT_FALSE(buffer.Write().empty());
	EXPECT_EQ(&data[3], buffer.Write().data);
	EXPECT_EQ(size_t(4), buffer.Write().size);

	/* consume [0..2]; after that, we can only write 5,
	   because the CircularBuffer class doesn't have
	   special code to rewind/reset an empty buffer */
	/* [..X.....] */
	buffer.Consume(3);
	EXPECT_TRUE(buffer.empty());
	EXPECT_FALSE(buffer.IsFull());
	EXPECT_EQ(size_t(0), buffer.GetSize());
	EXPECT_EQ(size_t(7), buffer.GetSpace());
	EXPECT_TRUE(buffer.Read().empty());
	EXPECT_FALSE(buffer.Write().empty());
	EXPECT_EQ(&data[3], buffer.Write().data);
	EXPECT_EQ(size_t(5), buffer.Write().size);
}
