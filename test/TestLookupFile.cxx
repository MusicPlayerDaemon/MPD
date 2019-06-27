#include "fs/LookupFile.hxx"
#include "util/Compiler.h"

#include <gtest/gtest.h>

#include <string.h>
#include <stdlib.h>

TEST(ArchiveTest, Lookup)
{
	EXPECT_THROW(LookupFile(Path::FromFS("")), std::system_error);

	EXPECT_FALSE(LookupFile(Path::FromFS(".")));

	EXPECT_FALSE(LookupFile(Path::FromFS("config.h")));

	EXPECT_THROW(LookupFile(Path::FromFS("src/foo/bar")), std::system_error);

	fclose(fopen("dummy", "w"));

	auto result = LookupFile(Path::FromFS("dummy/foo/bar"));
	EXPECT_TRUE(result);
	EXPECT_STREQ(result.archive.c_str(), "dummy");
	EXPECT_STREQ(result.inside.c_str(), "foo/bar");

	result = LookupFile(Path::FromFS("config.h/foo/bar"));
	EXPECT_TRUE(result);
	EXPECT_STREQ(result.archive.c_str(), "config.h");
	EXPECT_STREQ(result.inside.c_str(), "foo/bar");
}
