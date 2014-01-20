/*
 * Unit tests for playlist_check_translate_song().
 */

#include "config.h"
#include "PlaylistSong.hxx"
#include "DetachedSong.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/Tag.hxx"
#include "util/Domain.hxx"
#include "fs/AllocatedPath.hxx"
#include "ls.hxx"
#include "Log.hxx"
#include "DatabaseSong.hxx"
#include "Mapper.hxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string.h>
#include <stdio.h>

void
Log(const Domain &domain, gcc_unused LogLevel level, const char *msg)
{
	fprintf(stderr, "[%s] %s\n", domain.GetName(), msg);
}

bool
uri_supported_scheme(const char *uri)
{
	return memcmp(uri, "http://", 7) == 0;
}

const char *const music_directory = "/music";

const char *
map_to_relative_path(const char *path_utf8)
{
	size_t length = strlen(music_directory);
	if (memcmp(path_utf8, music_directory, length) == 0 &&
	    path_utf8[length] == '/')
		path_utf8 += length + 1;
	return path_utf8;
}

static void
BuildTag(gcc_unused TagBuilder &tag)
{
}

template<typename... Args>
static void
BuildTag(TagBuilder &tag, TagType type, const char *value, Args&&... args)
{
	tag.AddItem(type, value);
	BuildTag(tag, std::forward<Args>(args)...);
}

template<typename... Args>
static Tag
MakeTag(Args&&... args)
{
	TagBuilder tag;
	BuildTag(tag, std::forward<Args>(args)...);
	return tag.Commit();
}

static Tag
MakeTag1a()
{
	return MakeTag(TAG_ARTIST, "artist_a1", TAG_TITLE, "title_a1",
		       TAG_ALBUM, "album_a1");
}

static Tag
MakeTag1b()
{
	return MakeTag(TAG_ARTIST, "artist_b1", TAG_TITLE, "title_b1",
		       TAG_COMMENT, "comment_b1");
}

static Tag
MakeTag1c()
{
	return MakeTag(TAG_ARTIST, "artist_b1", TAG_TITLE, "title_b1",
		       TAG_COMMENT, "comment_b1", TAG_ALBUM, "album_a1");
}

static Tag
MakeTag2a()
{
	return MakeTag(TAG_ARTIST, "artist_a2", TAG_TITLE, "title_a2",
		       TAG_ALBUM, "album_a2");
}

static Tag
MakeTag2b()
{
	return MakeTag(TAG_ARTIST, "artist_b2", TAG_TITLE, "title_b2",
		       TAG_COMMENT, "comment_b2");
}

static Tag
MakeTag2c()
{
	return MakeTag(TAG_ARTIST, "artist_b2", TAG_TITLE, "title_b2",
		       TAG_COMMENT, "comment_b2", TAG_ALBUM, "album_a2");
}

static const char *uri1 = "/foo/bar.ogg";
static const char *uri2 = "foo/bar.ogg";

DetachedSong *
DatabaseDetachSong(const char *uri, gcc_unused Error &error)
{
	if (strcmp(uri, uri2) == 0)
		return new DetachedSong(uri, MakeTag2a());

	return nullptr;
}

bool
DetachedSong::Update()
{
	if (strcmp(GetURI(), uri1) == 0) {
		SetTag(MakeTag1a());
		return true;
	}

	return false;
}

static std::string
ToString(const Tag &tag)
{
	char buffer[64];
	sprintf(buffer, "%d", tag.time);

	std::string result = buffer;

	for (unsigned i = 0, n = tag.num_items; i != n; ++i) {
		const TagItem &item = *tag.items[i];
		result.push_back('|');
		result.append(tag_item_names[item.type]);
		result.push_back('=');
		result.append(item.value);
	}

	return result;
}

static std::string
ToString(const DetachedSong &song)
{
	std::string result = song.GetURI();
	result.push_back('|');

	char buffer[64];

	if (song.GetLastModified() > 0) {
		sprintf(buffer, "%lu", (unsigned long)song.GetLastModified());
		result.append(buffer);
	}

	result.push_back('|');

	if (song.GetStartMS() > 0) {
		sprintf(buffer, "%u", song.GetStartMS());
		result.append(buffer);
	}

	result.push_back('-');

	if (song.GetEndMS() > 0) {
		sprintf(buffer, "%u", song.GetEndMS());
		result.append(buffer);
	}

	result.push_back('|');

	result.append(ToString(song.GetTag()));

	return result;
}

static std::string
ToString(const DetachedSong *song)
{
	if (song == nullptr)
		return "nullptr";

	return ToString(*song);
}

class TranslateSongTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(TranslateSongTest);
	CPPUNIT_TEST(TestAbsoluteURI);
	CPPUNIT_TEST(TestInsecure);
	CPPUNIT_TEST(TestSecure);
	CPPUNIT_TEST(TestInDatabase);
	CPPUNIT_TEST(TestRelative);
	CPPUNIT_TEST_SUITE_END();

	void TestAbsoluteURI() {
		auto song1 = new DetachedSong("http://example.com/foo.ogg");
		auto song2 = playlist_check_translate_song(song1, "/ignored", false);
		CPPUNIT_ASSERT_EQUAL(song1, song2);
	}

	void TestInsecure() {
		/* illegal because secure=false */
		auto song1 = new DetachedSong(uri1);
		auto song2 = playlist_check_translate_song(song1, nullptr, false);
		CPPUNIT_ASSERT_EQUAL((DetachedSong *)nullptr, song2);
	}

	void TestSecure() {
		auto song1 = new DetachedSong(uri1, MakeTag1b());
		auto s1 = ToString(song1);
		auto se = ToString(DetachedSong(uri1, MakeTag1c()));
		auto song2 = playlist_check_translate_song(song1, "/ignored", true);
		CPPUNIT_ASSERT_EQUAL(se, ToString(song2));
		delete song2;
	}

	void TestInDatabase() {
		auto song1 = new DetachedSong("doesntexist");
		auto song2 = playlist_check_translate_song(song1, nullptr, false);
		CPPUNIT_ASSERT_EQUAL((DetachedSong *)nullptr, song2);

		song1 = new DetachedSong(uri2, MakeTag2b());
		auto s1 = ToString(song1);
		auto se = ToString(DetachedSong(uri2, MakeTag2c()));
		song2 = playlist_check_translate_song(song1, nullptr, false);
		CPPUNIT_ASSERT_EQUAL(se, ToString(song2));
		delete song2;

		song1 = new DetachedSong("/music/foo/bar.ogg", MakeTag2b());
		s1 = ToString(song1);
		se = ToString(DetachedSong(uri2, MakeTag2c()));
		song2 = playlist_check_translate_song(song1, nullptr, false);
		CPPUNIT_ASSERT_EQUAL(se, ToString(song2));
		delete song2;
	}

	void TestRelative() {
		/* map to music_directory */
		auto song1 = new DetachedSong("bar.ogg", MakeTag2b());
		auto s1 = ToString(song1);
		auto se = ToString(DetachedSong(uri2, MakeTag2c()));
		auto song2 = playlist_check_translate_song(song1, "/music/foo", false);
		CPPUNIT_ASSERT_EQUAL(se, ToString(song2));
		delete song2;

		/* illegal because secure=false */
		song1 = new DetachedSong("bar.ogg", MakeTag2b());
		song2 = playlist_check_translate_song(song1, "/foo", false);
		CPPUNIT_ASSERT_EQUAL((DetachedSong *)nullptr, song2);

		/* legal because secure=true */
		song1 = new DetachedSong("bar.ogg", MakeTag1b());
		s1 = ToString(song2);
		se = ToString(DetachedSong(uri1, MakeTag1c()));
		song2 = playlist_check_translate_song(song1, "/foo", true);
		CPPUNIT_ASSERT_EQUAL(se, ToString(song2));
		delete song2;

		/* relative to http:// */
		song1 = new DetachedSong("bar.ogg", MakeTag2a());
		s1 = ToString(song1);
		se = ToString(DetachedSong("http://example.com/foo/bar.ogg", MakeTag2a()));
		song2 = playlist_check_translate_song(song1, "http://example.com/foo", false);
		CPPUNIT_ASSERT_EQUAL(se, ToString(song2));
		delete song2;
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(TranslateSongTest);

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
