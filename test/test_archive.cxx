#include "archive/ArchiveLookup.hxx"
#include "util/Compiler.h"

#include <gtest/gtest.h>

#include <string.h>
#include <stdlib.h>

TEST(ArchiveTest, Lookup)
{
	char *path = strdup("");
	EXPECT_THROW(archive_lookup(path), std::system_error);
	free(path);

	path = strdup(".");
	EXPECT_FALSE(archive_lookup(path));
	free(path);

	path = strdup("config.h");
	EXPECT_FALSE(archive_lookup(path));
	free(path);

	path = strdup("src/foo/bar");
	EXPECT_THROW(archive_lookup(path), std::system_error);
	free(path);

	fclose(fopen("dummy", "w"));

	path = strdup("dummy/foo/bar");
	auto result = archive_lookup(path);
	EXPECT_TRUE(result);
	EXPECT_STREQ(result.archive.c_str(), "dummy");
	EXPECT_STREQ(result.inside.c_str(), "foo/bar");
	free(path);

	path = strdup("config.h/foo/bar");
	result = archive_lookup(path);
	EXPECT_TRUE(result);
	EXPECT_STREQ(result.archive.c_str(), "config.h");
	EXPECT_STREQ(result.inside.c_str(), "foo/bar");
	free(path);
}
