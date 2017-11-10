/*
 * Unit tests for src/util/
 */

#include "check.h"
#include "util/DivideString.hxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string.h>

class DivideStringTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(DivideStringTest);
	CPPUNIT_TEST(TestBasic);
	CPPUNIT_TEST(TestEmpty);
	CPPUNIT_TEST(TestFail);
	CPPUNIT_TEST(TestStrip);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestBasic() {
		constexpr char input[] = "foo.bar";
		const DivideString ds(input, '.');
		CPPUNIT_ASSERT(ds.IsDefined());
		CPPUNIT_ASSERT(!ds.empty());
		CPPUNIT_ASSERT_EQUAL(0, strcmp(ds.GetFirst(), "foo"));
		CPPUNIT_ASSERT_EQUAL(input + 4, ds.GetSecond());
	}

	void TestEmpty() {
		constexpr char input[] = ".bar";
		const DivideString ds(input, '.');
		CPPUNIT_ASSERT(ds.IsDefined());
		CPPUNIT_ASSERT(ds.empty());
		CPPUNIT_ASSERT_EQUAL(0, strcmp(ds.GetFirst(), ""));
		CPPUNIT_ASSERT_EQUAL(input + 1, ds.GetSecond());
	}

	void TestFail() {
		constexpr char input[] = "foo!bar";
		const DivideString ds(input, '.');
		CPPUNIT_ASSERT(!ds.IsDefined());
	}

	void TestStrip() {
		constexpr char input[] = " foo\t.\nbar\r";
		const DivideString ds(input, '.', true);
		CPPUNIT_ASSERT(ds.IsDefined());
		CPPUNIT_ASSERT(!ds.empty());
		CPPUNIT_ASSERT_EQUAL(0, strcmp(ds.GetFirst(), "foo"));
		CPPUNIT_ASSERT_EQUAL(input + 7, ds.GetSecond());
	}
};
