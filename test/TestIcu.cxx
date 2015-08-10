/*
 * Unit tests for src/util/
 */

#include "config.h"
#include "lib/icu/Converter.hxx"
#include "util/AllocatedString.hxx"
#include "util/StringAPI.hxx"
#include "util/Error.hxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string.h>
#include <stdlib.h>

#ifdef HAVE_ICU_CONVERTER

static const char *const invalid_utf8[] = {
	"\xfc",
};

struct StringPair {
	const char *utf8, *other;
};

static constexpr StringPair latin1_tests[] = {
	{ "foo", "foo" },
	{ "\xc3\xbc", "\xfc" },
};

class TestIcuConverter : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(TestIcuConverter);
	CPPUNIT_TEST(TestInvalidCharset);
	CPPUNIT_TEST(TestLatin1);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestInvalidCharset() {
		CPPUNIT_ASSERT_EQUAL((IcuConverter *)nullptr,
				     IcuConverter::Create("doesntexist",
							  IgnoreError()));
	}

	void TestLatin1() {
		IcuConverter *const converter =
			IcuConverter::Create("iso-8859-1", IgnoreError());
		CPPUNIT_ASSERT(converter != nullptr);

		for (const auto i : invalid_utf8) {
			auto f = converter->FromUTF8(i);
			CPPUNIT_ASSERT_EQUAL(true, f.IsNull());
		}

		for (const auto i : latin1_tests) {
			auto f = converter->FromUTF8(i.utf8);
			CPPUNIT_ASSERT_EQUAL(true, StringIsEqual(f.c_str(),
								 i.other));

			auto t = converter->ToUTF8(i.other);
			CPPUNIT_ASSERT_EQUAL(true, StringIsEqual(t.c_str(),
								 i.utf8));
		}

		delete converter;
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestIcuConverter);

#endif

int
main(gcc_unused int argc, gcc_unused char **argv)
{
#ifdef HAVE_ICU_CONVERTER
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
#else
	return EXIT_SUCCESS;
#endif
}
