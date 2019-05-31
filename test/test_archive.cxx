#include "archive/ArchiveLookup.hxx"
#include "util/Compiler.h"

#include <gtest/gtest.h>

#include <string.h>
#include <stdlib.h>

TEST(ArchiveTest, Lookup)
{
	const char *archive, *inpath;

	char *path = strdup("");
	EXPECT_THROW(archive_lookup(path, &archive, &inpath),
		     std::system_error);
	free(path);

	path = strdup(".");
	EXPECT_FALSE(archive_lookup(path, &archive, &inpath));
	free(path);

	path = strdup("config.h");
	EXPECT_FALSE(archive_lookup(path, &archive, &inpath));
	free(path);

	path = strdup("src/foo/bar");
	EXPECT_THROW(archive_lookup(path, &archive, &inpath),
		     std::system_error);
	free(path);

	fclose(fopen("dummy", "w"));

	path = strdup("dummy/foo/bar");
	EXPECT_TRUE(archive_lookup(path, &archive, &inpath));
	EXPECT_EQ((const char *)path, archive);
	EXPECT_STREQ(archive, "dummy");
	EXPECT_STREQ(inpath, "foo/bar");
	free(path);

	path = strdup("config.h/foo/bar");
	EXPECT_TRUE(archive_lookup(path, &archive, &inpath));
	EXPECT_EQ((const char *)path, archive);
	EXPECT_STREQ(archive, "config.h");
	EXPECT_STREQ(inpath, "foo/bar");
	free(path);
}
