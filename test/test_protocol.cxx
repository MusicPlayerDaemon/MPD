#include "config.h"
#include "protocol/ArgParser.hxx"
#include "protocol/Result.hxx"
#include "Compiler.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <stdlib.h>

static enum ack last_error = ack(-1);

void
command_error(gcc_unused Client &client, enum ack error,
	      gcc_unused const char *fmt, ...)
{
	last_error = error;
}

class ArgParserTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(ArgParserTest);
	CPPUNIT_TEST(TestRange);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestRange();
};

void
ArgParserTest::TestRange()
{
	Client &client = *(Client *)nullptr;
	unsigned a, b;

	CPPUNIT_ASSERT(check_range(client, &a, &b, "1"));
	CPPUNIT_ASSERT_EQUAL(1u, a);
	CPPUNIT_ASSERT_EQUAL(2u, b);

	CPPUNIT_ASSERT(check_range(client, &a, &b, "1:5"));
	CPPUNIT_ASSERT_EQUAL(1u, a);
	CPPUNIT_ASSERT_EQUAL(5u, b);

	CPPUNIT_ASSERT(check_range(client, &a, &b, "1:"));
	CPPUNIT_ASSERT_EQUAL(1u, a);
	CPPUNIT_ASSERT(b >= 999999u);

	CPPUNIT_ASSERT(!check_range(client, &a, &b, "-2"));
	CPPUNIT_ASSERT_EQUAL(ACK_ERROR_ARG, last_error);
}

CPPUNIT_TEST_SUITE_REGISTRATION(ArgParserTest);

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
