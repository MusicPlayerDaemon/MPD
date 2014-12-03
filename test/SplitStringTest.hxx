/*
 * Unit tests for src/util/
 */

#include "check.h"
#include "util/SplitString.hxx"
#include "util/Macros.hxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string.h>

class SplitStringTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(SplitStringTest);
	CPPUNIT_TEST(TestBasic);
	CPPUNIT_TEST(TestStrip);
	CPPUNIT_TEST(TestNoStrip);
	CPPUNIT_TEST(TestEmpty);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestBasic() {
		constexpr char input[] = "foo.bar";
		const char *const output[] = { "foo", "bar" };
		size_t i = 0;
		for (auto p : SplitString(input, '.')) {
			CPPUNIT_ASSERT(i < ARRAY_SIZE(output));
			CPPUNIT_ASSERT(p == output[i]);
			++i;
		}

		CPPUNIT_ASSERT_EQUAL(ARRAY_SIZE(output), i);
	}

	void TestStrip() {
		constexpr char input[] = " foo\t.\r\nbar\r\n2";
		const char *const output[] = { "foo", "bar\r\n2" };
		size_t i = 0;
		for (auto p : SplitString(input, '.')) {
			CPPUNIT_ASSERT(i < ARRAY_SIZE(output));
			CPPUNIT_ASSERT(p == output[i]);
			++i;
		}

		CPPUNIT_ASSERT_EQUAL(ARRAY_SIZE(output), i);
	}

	void TestNoStrip() {
		constexpr char input[] = " foo\t.\r\nbar\r\n2";
		const char *const output[] = { " foo\t", "\r\nbar\r\n2" };
		size_t i = 0;
		for (auto p : SplitString(input, '.', false)) {
			CPPUNIT_ASSERT(i < ARRAY_SIZE(output));
			CPPUNIT_ASSERT(p == output[i]);
			++i;
		}

		CPPUNIT_ASSERT_EQUAL(ARRAY_SIZE(output), i);
	}

	void TestEmpty() {
		CPPUNIT_ASSERT(SplitString("", '.').empty());
	}
};
