#include "config.h"
#include "protocol/ArgParser.hxx"
#include "protocol/Ack.hxx"
#include "Compiler.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <stdlib.h>

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
	RangeArg range = ParseCommandArgRange("1");
	CPPUNIT_ASSERT_EQUAL(1u, range.start);
	CPPUNIT_ASSERT_EQUAL(2u, range.end);

	range = ParseCommandArgRange("1:5");
	CPPUNIT_ASSERT_EQUAL(1u, range.start);
	CPPUNIT_ASSERT_EQUAL(5u, range.end);

	range = ParseCommandArgRange("1:");
	CPPUNIT_ASSERT_EQUAL(1u, range.start);
	CPPUNIT_ASSERT(range.end >= 999999u);

	try {
		range = ParseCommandArgRange("-2");
		CPPUNIT_ASSERT(false);
	} catch (const ProtocolError &) {
		CPPUNIT_ASSERT(true);
	}
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
