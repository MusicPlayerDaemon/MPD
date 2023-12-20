/*
 * Unit tests for class IcyMetaDataParser.
 */

/* include the .cxx file to get access to internal functions */
#include "tag/IcyMetaDataParser.cxx"

#include <gtest/gtest.h>

#include <string>

#include <string.h>

#ifdef HAVE_ICU_CONVERTER

static std::unique_ptr<Tag>
icy_parse_tag(std::string_view p) noexcept
{
	return icy_parse_tag(nullptr, p);
}

#endif

static void
CompareTagTitle(const Tag &tag, const std::string &title)
{
	EXPECT_EQ(uint16_t(1), tag.num_items);

	const TagItem &item = *tag.items[0];
	EXPECT_EQ(TAG_TITLE, item.type);
	EXPECT_EQ(title, std::string(item.value));
}

static void
TestIcyParserTitle(const char *input, const char *title)
{
	const auto tag = icy_parse_tag(input);
	CompareTagTitle(*tag, title);
}

static void
TestIcyParserEmpty(const char *input)
{
	const auto tag = icy_parse_tag(input);
	EXPECT_EQ(uint16_t(0), tag->num_items);
}

TEST(IcyMetadataParserTest, Basic)
{
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
