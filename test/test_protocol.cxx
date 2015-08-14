#include "config.h"
#include "protocol/ArgParser.hxx"
#include "client/Response.hxx"
#include "Compiler.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <stdlib.h>

static enum ack last_error = ack(-1);

void
Response::Error(enum ack code, gcc_unused const char *msg)
{
	last_error = code;
}

void
Response::FormatError(enum ack code, gcc_unused const char *fmt, ...)
{
	last_error = code;
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
	Response r(client, 0);

	RangeArg range;

	CPPUNIT_ASSERT(ParseCommandArg(r, range, "1"));
	CPPUNIT_ASSERT_EQUAL(1u, range.start);
	CPPUNIT_ASSERT_EQUAL(2u, range.end);

	CPPUNIT_ASSERT(ParseCommandArg(r, range, "1:5"));
	CPPUNIT_ASSERT_EQUAL(1u, range.start);
	CPPUNIT_ASSERT_EQUAL(5u, range.end);

	CPPUNIT_ASSERT(ParseCommandArg(r, range, "1:"));
	CPPUNIT_ASSERT_EQUAL(1u, range.start);
	CPPUNIT_ASSERT(range.end >= 999999u);

	CPPUNIT_ASSERT(!ParseCommandArg(r, range, "-2"));
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
