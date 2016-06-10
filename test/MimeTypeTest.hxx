/*
 * Unit tests for src/util/
 */

#include "check.h"
#include "util/MimeType.hxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string.h>

class MimeTypeTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(MimeTypeTest);
	CPPUNIT_TEST(TestBase);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestBase() {
		CPPUNIT_ASSERT("" == GetMimeTypeBase(""));
		CPPUNIT_ASSERT("" == GetMimeTypeBase(";"));
		CPPUNIT_ASSERT("foo" == GetMimeTypeBase("foo"));
		CPPUNIT_ASSERT("foo/bar" == GetMimeTypeBase("foo/bar"));
		CPPUNIT_ASSERT("foo/bar" == GetMimeTypeBase("foo/bar;"));
		CPPUNIT_ASSERT("foo/bar" == GetMimeTypeBase("foo/bar; x=y"));
		CPPUNIT_ASSERT("foo/bar" == GetMimeTypeBase("foo/bar;x=y"));
	}
};
