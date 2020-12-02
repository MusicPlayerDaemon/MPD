/*
 * Unit tests for src/util/
 */

#include "util/UriQueryParser.hxx"
#include "util/StringView.hxx"

#include <gtest/gtest.h>

static bool
operator==(StringView a, StringView b)
{
	if (a.IsNull() || b.IsNull())
		return a.IsNull() == b.IsNull();

	return a.Equals(b);
}

TEST(UriQueryParser, UriFindRawQueryParameter)
{
	const char *q = "foo=1&bar=2&quoted=%20%00+%%&empty1&empty2=";
	EXPECT_EQ(UriFindRawQueryParameter(q, "doesntexist"),
		  (const char *)nullptr);
	EXPECT_EQ(UriFindRawQueryParameter(q, "foo"),
		  "1");
	EXPECT_EQ(UriFindRawQueryParameter(q, "bar"),
		  "2");
	EXPECT_EQ(UriFindRawQueryParameter(q, "quoted"),
		  "%20%00+%%");
	EXPECT_EQ(UriFindRawQueryParameter(q, "empty1"),
		  "");
	EXPECT_EQ(UriFindRawQueryParameter(q, "empty2"),
		  "");
}
