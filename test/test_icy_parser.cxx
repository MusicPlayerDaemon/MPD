/*
 * Unit tests for class IcyMetaDataParser.
 */

#include "config.h"

/* include the .cxx file to get access to internal functions */
#include "IcyMetaDataParser.cxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string>

#include <string.h>

static Tag *
icy_parse_tag(const char *p)
{
	char *q = strdup(p);
	Tag *tag = icy_parse_tag(q, q + strlen(q));
	free(q);
	return tag;
}

static void
CompareTagTitle(const Tag &tag, const std::string &title)
{
	CPPUNIT_ASSERT_EQUAL(uint16_t(1), tag.num_items);

	const TagItem &item = *tag.items[0];
	CPPUNIT_ASSERT_EQUAL(TAG_TITLE, item.type);
	CPPUNIT_ASSERT_EQUAL(title, std::string(item.value));
}

static void
TestIcyParserTitle(const char *input, const char *title)
{
	Tag *tag = icy_parse_tag(input);
	CompareTagTitle(*tag, title);
	delete tag;
}

static void
TestIcyParserEmpty(const char *input)
{
	Tag *tag = icy_parse_tag(input);
	CPPUNIT_ASSERT_EQUAL(uint16_t(0), tag->num_items);
	delete tag;
}

class IcyTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(IcyTest);
	CPPUNIT_TEST(TestIcyMetadataParser);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestIcyMetadataParser() {
		TestIcyParserEmpty("foo=bar;");
		TestIcyParserTitle("StreamTitle='foo bar'", "foo bar");
		TestIcyParserTitle("StreamTitle='foo bar';", "foo bar");
		TestIcyParserTitle("StreamTitle='foo\"bar';", "foo\"bar");
		TestIcyParserTitle("StreamTitle='foo=bar';", "foo=bar");
		TestIcyParserTitle("a=b;StreamTitle='foo';", "foo");
		TestIcyParserTitle("a=;StreamTitle='foo';", "foo");
		TestIcyParserTitle("a=b;StreamTitle='foo';c=d", "foo");
		TestIcyParserTitle("a=b;StreamTitle='foo'", "foo");
		TestIcyParserTitle("a='b;c';StreamTitle='foo;bar'", "foo;bar");
		TestIcyParserTitle("a='b'c';StreamTitle='foo'bar'", "foo'bar");
		TestIcyParserTitle("StreamTitle='fo'o'b'ar';a='b'c'd'", "fo'o'b'ar");
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(IcyTest);

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
