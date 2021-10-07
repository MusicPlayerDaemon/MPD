/*
 * Unit tests for src/util/UriRelative.hxx
 */

#include "util/UriRelative.hxx"

#include <gtest/gtest.h>

TEST(UriRelative, IsChild)
{
	static constexpr struct {
		const char *parent;
		const char *child;
		bool is_child;
		bool is_child_or_same;
	} tests[] = {
		{ "/foo", "/foo", false, true },
		{ "/foo", "/foo/bar", true, true },
		{ "/foo/", "/foo/bar", true, true },
		{ "/foo/", "/foo/", false, true },
		{ "/foo/", "/foo", false, false },
		{ "/bar", "/foo", false, false },
		{ "/foo", "/foobar", false, false },
	};

	for (const auto &i : tests) {
		EXPECT_EQ(uri_is_child(i.parent, i.child), i.is_child);
		EXPECT_EQ(uri_is_child_or_same(i.parent, i.child),
			  i.is_child_or_same);
	}
}

TEST(UriRelative, ApplyBase)
{
	static constexpr struct {
		const char *uri;
		const char *base;
		const char *result;
	} tests[] = {
		{ "foo", "bar", "bar/foo" },
		{ "foo", "/bar", "/bar/foo" },
		{ "/foo", "/bar", "/foo" },
		{ "/foo", "bar", "/foo" },
		{ "/foo", "http://localhost/bar", "http://localhost/foo" },
		{ "/foo", "http://localhost/", "http://localhost/foo" },
		{ "/foo", "http://localhost", "http://localhost/foo" },
	};

	for (const auto &i : tests) {
		EXPECT_STREQ(uri_apply_base(i.uri, i.base).c_str(), i.result);
	}
}

TEST(UriRelative, ApplyRelative)
{
	static constexpr struct {
		const char *relative;
		const char *base;
		const char *result;
	} tests[] = {
		{ "", "bar", "bar" },
		{ ".", "bar", "" },
		{ "foo", "bar", "foo" },
		{ "", "/bar", "/bar" },
		{ ".", "/bar", "/" },
		{ "foo", "/bar", "/foo" },
		{ "", "/bar/", "/bar/" },
		{ ".", "/bar/", "/bar/" },
		{ ".", "/bar/foo", "/bar/" },
		{ "/foo", "/bar/", "/foo" },
		{ "foo", "/bar/", "/bar/foo" },
		{ "../foo", "/bar/", "/foo" },
		{ "./foo", "/bar/", "/bar/foo" },
		{ "./../foo", "/bar/", "/foo" },
		{ ".././foo", "/bar/", "/foo" },
		{ "../../foo", "/bar/", "" },
		{ "/foo", "http://localhost/bar/", "http://localhost/foo" },
		{ "/foo", "http://localhost/bar", "http://localhost/foo" },
		{ "/foo", "http://localhost/", "http://localhost/foo" },
		{ "/foo", "http://localhost", "http://localhost/foo" },
		{ "/", "http://localhost", "http://localhost/" },
		{ "/", "http://localhost/bar", "http://localhost/" },
		{ "/", "http://localhost/bar/", "http://localhost/" },
		{ "/", "http://localhost/bar/foo", "http://localhost/" },
		{ "../foo", "http://localhost/bar/", "http://localhost/foo" },
		{ "../foo", "http://localhost/bar", "" },
		{ "../foo", "http://localhost/", "" },
		{ "../foo", "http://localhost", "" },
		{ ".", "http://localhost", "http://localhost/" },
		{ "./foo", "http://localhost", "http://localhost/foo" },
	};

	for (const auto &i : tests) {
		EXPECT_STREQ(uri_apply_relative(i.relative, i.base).c_str(),
			     i.result);
	}
}
