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
	CPPUNIT_TEST(TestParameters);
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

	void TestParameters() {
		CPPUNIT_ASSERT(ParseMimeTypeParameters("").empty());
		CPPUNIT_ASSERT(ParseMimeTypeParameters("foo/bar").empty());
		CPPUNIT_ASSERT(ParseMimeTypeParameters("foo/bar;").empty());
		CPPUNIT_ASSERT(ParseMimeTypeParameters("foo/bar;garbage").empty());
		CPPUNIT_ASSERT(ParseMimeTypeParameters("foo/bar; garbage").empty());

		auto p = ParseMimeTypeParameters("foo/bar;a=b");
		CPPUNIT_ASSERT(!p.empty());
		CPPUNIT_ASSERT(p["a"] == "b");
		CPPUNIT_ASSERT(p.size() == 1);

		p = ParseMimeTypeParameters("foo/bar; a=b;c;d=e ; f=g ");
		CPPUNIT_ASSERT(!p.empty());
		CPPUNIT_ASSERT(p["a"] == "b");
		CPPUNIT_ASSERT(p["d"] == "e");
		CPPUNIT_ASSERT(p["f"] == "g");
		CPPUNIT_ASSERT(p.size() == 3);
	}
};
