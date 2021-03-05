#include "fs/LookupFile.hxx"
#include "util/Compiler.h"

#include <gtest/gtest.h>

#include <string.h>
#include <stdlib.h>

TEST(ArchiveTest, Lookup)
{
	EXPECT_THROW(LookupFile(Path::FromFS(PATH_LITERAL(""))), std::system_error);

	EXPECT_FALSE(LookupFile(Path::FromFS(PATH_LITERAL("."))));

	EXPECT_FALSE(LookupFile(Path::FromFS(PATH_LITERAL("config.h"))));

	EXPECT_THROW(LookupFile(Path::FromFS(PATH_LITERAL("src/foo/bar"))), std::system_error);

	fclose(fopen("dummy", "w"));

	auto result = LookupFile(Path::FromFS(PATH_LITERAL("dummy/foo/bar")));
	EXPECT_TRUE(result);
	EXPECT_STREQ(result.archive.c_str(), PATH_LITERAL("dummy"));
	EXPECT_STREQ(result.inside.c_str(), PATH_LITERAL("foo/bar"));

	result = LookupFile(Path::FromFS(PATH_LITERAL("config.h/foo/bar")));
	EXPECT_TRUE(result);
	EXPECT_STREQ(result.archive.c_str(), PATH_LITERAL("config.h"));
	EXPECT_STREQ(result.inside.c_str(), PATH_LITERAL("foo/bar"));
}
