/*
 * Unit tests for class CircularBuffer.
 */

#include "config.h"
#include "util/CircularBuffer.hxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string.h>
#include <stdlib.h>

class TestCircularBuffer : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(TestCircularBuffer);
	CPPUNIT_TEST(TestIt);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestIt() {
		static size_t N = 8;
		int data[N];
		CircularBuffer<int> buffer(data, N);

		CPPUNIT_ASSERT_EQUAL(size_t(N), buffer.GetCapacity());

		/* '.' = empty; 'O' = occupied; 'X' = blocked */

		/* checks on empty buffer */
		/* [.......X] */
		CPPUNIT_ASSERT_EQUAL(true, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(size_t(0), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(7), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(true, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Write().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(&data[0], buffer.Write().data);
		CPPUNIT_ASSERT_EQUAL(size_t(7), buffer.Write().size);

		/* append one element */
		/* [O......X] */
		buffer.Append(1);
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(size_t(1), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(6), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(size_t(1), buffer.Read().size);
		CPPUNIT_ASSERT_EQUAL(&data[0], buffer.Read().data);
		CPPUNIT_ASSERT_EQUAL(false, buffer.Write().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(&data[1], buffer.Write().data);
		CPPUNIT_ASSERT_EQUAL(size_t(6), buffer.Write().size);

		/* append 6 elements, buffer is now full */
		/* [OOOOOOOX] */
		buffer.Append(6);
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(true, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(size_t(7), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(0), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(size_t(7), buffer.Read().size);
		CPPUNIT_ASSERT_EQUAL(&data[0], buffer.Read().data);
		CPPUNIT_ASSERT_EQUAL(true, buffer.Write().IsEmpty());

		/* consume [0]; can append one at [7] */
		/* [XOOOOOO.] */
		buffer.Consume(1);
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(size_t(6), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(1), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(size_t(6), buffer.Read().size);
		CPPUNIT_ASSERT_EQUAL(&data[1], buffer.Read().data);
		CPPUNIT_ASSERT_EQUAL(false, buffer.Write().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(&data[7], buffer.Write().data);
		CPPUNIT_ASSERT_EQUAL(size_t(1), buffer.Write().size);

		/* append one element; [0] is still empty but cannot
		   be written to because head==1 */
		/* [XOOOOOOO] */
		buffer.Append(1);
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(true, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(size_t(7), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(0), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(size_t(7), buffer.Read().size);
		CPPUNIT_ASSERT_EQUAL(&data[1], buffer.Read().data);
		CPPUNIT_ASSERT_EQUAL(true, buffer.Write().IsEmpty());

		/* consume [1..3]; can append [0..2] */
		/* [...XOOOO] */
		buffer.Consume(3);
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(size_t(4), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(3), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(size_t(4), buffer.Read().size);
		CPPUNIT_ASSERT_EQUAL(&data[4], buffer.Read().data);
		CPPUNIT_ASSERT_EQUAL(false, buffer.Write().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(&data[0], buffer.Write().data);
		CPPUNIT_ASSERT_EQUAL(size_t(3), buffer.Write().size);

		/* append [0..1] */
		/* [OO.XOOOO] */
		buffer.Append(2);
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(size_t(6), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(1), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(size_t(4), buffer.Read().size);
		CPPUNIT_ASSERT_EQUAL(&data[4], buffer.Read().data);
		CPPUNIT_ASSERT_EQUAL(false, buffer.Write().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(&data[2], buffer.Write().data);
		CPPUNIT_ASSERT_EQUAL(size_t(1), buffer.Write().size);

		/* append [2] */
		/* [OOOXOOOO] */
		buffer.Append(1);
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(true, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(size_t(7), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(0), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(size_t(4), buffer.Read().size);
		CPPUNIT_ASSERT_EQUAL(&data[4], buffer.Read().data);
		CPPUNIT_ASSERT_EQUAL(true, buffer.Write().IsEmpty());

		/* consume [4..7] */
		/* [OOO....X] */
		buffer.Consume(4);
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(size_t(3), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(4), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(size_t(3), buffer.Read().size);
		CPPUNIT_ASSERT_EQUAL(&data[0], buffer.Read().data);
		CPPUNIT_ASSERT_EQUAL(false, buffer.Write().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(&data[3], buffer.Write().data);
		CPPUNIT_ASSERT_EQUAL(size_t(4), buffer.Write().size);

		/* consume [0..2]; after that, we can only write 5,
		   because the CircularBuffer class doesn't have
		   special code to rewind/reset an empty buffer */
		/* [..X.....] */
		buffer.Consume(3);
		CPPUNIT_ASSERT_EQUAL(true, buffer.IsEmpty());
		CPPUNIT_ASSERT_EQUAL(false, buffer.IsFull());
		CPPUNIT_ASSERT_EQUAL(size_t(0), buffer.GetSize());
		CPPUNIT_ASSERT_EQUAL(size_t(7), buffer.GetSpace());
		CPPUNIT_ASSERT_EQUAL(true, buffer.Read().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(false, buffer.Write().IsEmpty());
		CPPUNIT_ASSERT_EQUAL(&data[3], buffer.Write().data);
		CPPUNIT_ASSERT_EQUAL(size_t(5), buffer.Write().size);
	}
};
