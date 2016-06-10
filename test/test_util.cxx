/*
 * Unit tests for src/util/
 */

#include "config.h"
#include "DivideStringTest.hxx"
#include "SplitStringTest.hxx"
#include "UriUtilTest.hxx"
#include "MimeTypeTest.hxx"
#include "TestCircularBuffer.hxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION(DivideStringTest);
CPPUNIT_TEST_SUITE_REGISTRATION(SplitStringTest);
CPPUNIT_TEST_SUITE_REGISTRATION(UriUtilTest);
CPPUNIT_TEST_SUITE_REGISTRATION(MimeTypeTest);
CPPUNIT_TEST_SUITE_REGISTRATION(TestCircularBuffer);

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
