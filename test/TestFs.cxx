/*
 * Unit tests for src/fs/
 */

#include "config.h"
#include "fs/Glob.hxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string.h>
#include <stdlib.h>

#ifdef HAVE_CLASS_GLOB

class TestGlob : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(TestGlob);
	CPPUNIT_TEST(Basic);
	CPPUNIT_TEST(Asterisk);
	CPPUNIT_TEST(QuestionMark);
	CPPUNIT_TEST(Wildcard);
	CPPUNIT_TEST(PrefixWildcard);
	CPPUNIT_TEST(SuffixWildcard);
	CPPUNIT_TEST_SUITE_END();

public:
	void Basic() {
		const Glob glob("foo");
		CPPUNIT_ASSERT(glob.Check("foo"));
		CPPUNIT_ASSERT(!glob.Check("fooo"));
		CPPUNIT_ASSERT(!glob.Check("_foo"));
		CPPUNIT_ASSERT(!glob.Check("a/foo"));
		CPPUNIT_ASSERT(!glob.Check(""));
		CPPUNIT_ASSERT(!glob.Check("*"));
	}

	void Asterisk() {
		const Glob glob("*");
		CPPUNIT_ASSERT(glob.Check("foo"));
		CPPUNIT_ASSERT(glob.Check("bar"));
		CPPUNIT_ASSERT(glob.Check("*"));
		CPPUNIT_ASSERT(glob.Check("?"));
	}

	void QuestionMark() {
		const Glob glob("foo?bar");
		CPPUNIT_ASSERT(glob.Check("foo_bar"));
		CPPUNIT_ASSERT(glob.Check("foo?bar"));
		CPPUNIT_ASSERT(glob.Check("foo bar"));
		CPPUNIT_ASSERT(!glob.Check("foobar"));
		CPPUNIT_ASSERT(!glob.Check("foo__bar"));
	}

	void Wildcard() {
		const Glob glob("foo*bar");
		CPPUNIT_ASSERT(glob.Check("foo_bar"));
		CPPUNIT_ASSERT(glob.Check("foo?bar"));
		CPPUNIT_ASSERT(glob.Check("foo bar"));
		CPPUNIT_ASSERT(glob.Check("foobar"));
		CPPUNIT_ASSERT(glob.Check("foo__bar"));
		CPPUNIT_ASSERT(!glob.Check("_foobar"));
		CPPUNIT_ASSERT(!glob.Check("foobar_"));
	}

	void PrefixWildcard() {
		const Glob glob("*bar");
		CPPUNIT_ASSERT(glob.Check("foo_bar"));
		CPPUNIT_ASSERT(glob.Check("foo?bar"));
		CPPUNIT_ASSERT(glob.Check("foo bar"));
		CPPUNIT_ASSERT(glob.Check("foobar"));
		CPPUNIT_ASSERT(glob.Check("foo__bar"));
		CPPUNIT_ASSERT(glob.Check("_foobar"));
		CPPUNIT_ASSERT(glob.Check("bar"));
		CPPUNIT_ASSERT(!glob.Check("foobar_"));
	}

	void SuffixWildcard() {
		const Glob glob("foo*");
		CPPUNIT_ASSERT(glob.Check("foo_bar"));
		CPPUNIT_ASSERT(glob.Check("foo?bar"));
		CPPUNIT_ASSERT(glob.Check("foo bar"));
		CPPUNIT_ASSERT(glob.Check("foobar"));
		CPPUNIT_ASSERT(glob.Check("foo__bar"));
		CPPUNIT_ASSERT(glob.Check("foobar_"));
		CPPUNIT_ASSERT(glob.Check("foo"));
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestGlob);

#endif

int
main(gcc_unused int argc, gcc_unused char **argv)
{
#ifdef HAVE_CLASS_GLOB
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
#else
	return EXIT_SUCCESS;
#endif
}
