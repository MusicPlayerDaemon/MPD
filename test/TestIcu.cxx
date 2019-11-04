/*
 * Unit tests for src/util/
 */

#include "config.h"
#include "lib/icu/Converter.hxx"
#include "util/AllocatedString.hxx"

#include <gtest/gtest.h>

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

TEST(IcuConverter, InvalidCharset)
{
	EXPECT_ANY_THROW(IcuConverter::Create("doesntexist"));
}

TEST(IcuConverter, Latin1)
{
	const auto converter = IcuConverter::Create("iso-8859-1");
	ASSERT_NE(converter, nullptr);

	for (const auto i : invalid_utf8) {
		EXPECT_ANY_THROW(converter->FromUTF8(i));
	}

	for (const auto i : latin1_tests) {
		auto f = converter->FromUTF8(i.utf8);
		EXPECT_STREQ(f.c_str(), i.other);

		auto t = converter->ToUTF8(i.other);
		EXPECT_STREQ(t.c_str(), i.utf8);
	}
}

#endif
