/*
 * Unit tests for mixramp_interpolate()
 */

#include "config.h"
#include "CrossFade.cxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string.h>

class MixRampTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(MixRampTest);
	CPPUNIT_TEST(TestInterpolate);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestInterpolate() {
		const char *input = "1.0 0.00;3.0 0.10;6.0 2.50;";

		char *foo = strdup(input);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(double(0),
					     mixramp_interpolate(foo, 0),
					     0.05);
		free(foo);

		foo = strdup(input);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(float(0),
					     mixramp_interpolate(foo, 1),
					     0.005);
		free(foo);

		foo = strdup(input);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(float(0.1),
					     mixramp_interpolate(foo, 3),
					     0.005);
		free(foo);

		foo = strdup(input);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(float(2.5),
					     mixramp_interpolate(foo, 6),
					     0.01);
		free(foo);

		foo = strdup(input);
		CPPUNIT_ASSERT(mixramp_interpolate(foo, 6.1) < 0);
		free(foo);

		foo = strdup(input);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(float(0.05),
					     mixramp_interpolate(foo, 2),
					     0.05);
		free(foo);

		foo = strdup(input);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(float(1.3),
					     mixramp_interpolate(foo, 4.5),
					     0.05);
		free(foo);

		foo = strdup(input);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(float(0.9),
					     mixramp_interpolate(foo, 4),
					     0.05);
		free(foo);

		foo = strdup(input);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(float(1.7),
					     mixramp_interpolate(foo, 5),
					     0.05);
		free(foo);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(MixRampTest);

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
