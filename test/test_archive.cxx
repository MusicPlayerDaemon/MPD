#include "archive/ArchiveLookup.hxx"
#include "util/Compiler.h"

#include <gtest/gtest.h>

#include <string.h>
#include <stdlib.h>

TEST(ArchiveTest, Lookup)
{
	EXPECT_THROW(archive_lookup(Path::FromFS("")), std::system_error);

	EXPECT_FALSE(archive_lookup(Path::FromFS(".")));

	EXPECT_FALSE(archive_lookup(Path::FromFS("config.h")));

	EXPECT_THROW(archive_lookup(Path::FromFS("src/foo/bar")), std::system_error);

	fclose(fopen("dummy", "w"));

	auto result = archive_lookup(Path::FromFS("dummy/foo/bar"));
	EXPECT_TRUE(result);
	EXPECT_STREQ(result.archive.c_str(), "dummy");
	EXPECT_STREQ(result.inside.c_str(), "foo/bar");

	result = archive_lookup(Path::FromFS("config.h/foo/bar"));
	EXPECT_TRUE(result);
	EXPECT_STREQ(result.archive.c_str(), "config.h");
	EXPECT_STREQ(result.inside.c_str(), "foo/bar");
}
