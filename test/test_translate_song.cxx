/*
 * Unit tests for playlist_check_translate_song().
 */

#include "config.h"
#include "playlist/PlaylistSong.hxx"
#include "DetachedSong.hxx"
#include "SongLoader.hxx"
#include "client/Client.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/Tag.hxx"
#include "util/Domain.hxx"
#include "fs/AllocatedPath.hxx"
#include "ls.hxx"
#include "Log.hxx"
#include "db/DatabaseSong.hxx"
#include "storage/plugins/LocalStorage.hxx"
#include "util/Error.hxx"
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

static const char *const music_directory = "/music";
static Storage *storage;

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
DatabaseDetachSong(gcc_unused const Database &db,
		   gcc_unused const Storage &_storage,
		   const char *uri,
		   gcc_unused Error &error)
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

const Database *
Client::GetDatabase(gcc_unused Error &error) const
{
	return reinterpret_cast<const Database *>(this);
}

const Storage *
Client::GetStorage() const
{
	return ::storage;
}

bool
Client::AllowFile(gcc_unused Path path_fs, gcc_unused Error &error) const
{
	/* always return false, so a SongLoader with a non-nullptr
	   Client pointer will be regarded "insecure", while one with
	   client==nullptr will allow all files */
	return false;
}

static std::string
ToString(const Tag &tag)
{
	std::string result;

	if (!tag.duration.IsNegative()) {
		char buffer[64];
		sprintf(buffer, "%d", tag.duration.ToMS());
		result.append(buffer);
	}

	for (const auto &item : tag) {
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

	if (song.GetStartTime().IsPositive()) {
		sprintf(buffer, "%u", song.GetStartTime().ToMS());
		result.append(buffer);
	}

	result.push_back('-');

	if (song.GetEndTime().IsPositive()) {
		sprintf(buffer, "%u", song.GetEndTime().ToMS());
		result.append(buffer);
	}

	result.push_back('|');

	result.append(ToString(song.GetTag()));

	return result;
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
		DetachedSong song1("http://example.com/foo.ogg");
		auto se = ToString(song1);
		const SongLoader loader(nullptr, nullptr);
		CPPUNIT_ASSERT(playlist_check_translate_song(song1, "/ignored",
							     loader));
		CPPUNIT_ASSERT_EQUAL(se, ToString(song1));
	}

	void TestInsecure() {
		/* illegal because secure=false */
		DetachedSong song1 (uri1);
		const SongLoader loader(*reinterpret_cast<const Client *>(1));
		CPPUNIT_ASSERT(!playlist_check_translate_song(song1, nullptr,
							      loader));
	}

	void TestSecure() {
		DetachedSong song1(uri1, MakeTag1b());
		auto s1 = ToString(song1);
		auto se = ToString(DetachedSong(uri1, MakeTag1c()));

		const SongLoader loader(nullptr, nullptr);
		CPPUNIT_ASSERT(playlist_check_translate_song(song1, "/ignored",
							     loader));
		CPPUNIT_ASSERT_EQUAL(se, ToString(song1));
	}

	void TestInDatabase() {
		const SongLoader loader(reinterpret_cast<const Database *>(1),
					storage);

		DetachedSong song1("doesntexist");
		CPPUNIT_ASSERT(!playlist_check_translate_song(song1, nullptr,
							      loader));

		DetachedSong song2(uri2, MakeTag2b());
		auto s1 = ToString(song2);
		auto se = ToString(DetachedSong(uri2, MakeTag2c()));
		CPPUNIT_ASSERT(playlist_check_translate_song(song2, nullptr,
							     loader));
		CPPUNIT_ASSERT_EQUAL(se, ToString(song2));

		DetachedSong song3("/music/foo/bar.ogg", MakeTag2b());
		s1 = ToString(song3);
		se = ToString(DetachedSong(uri2, MakeTag2c()));
		CPPUNIT_ASSERT(playlist_check_translate_song(song3, nullptr,
							     loader));
		CPPUNIT_ASSERT_EQUAL(se, ToString(song3));
	}

	void TestRelative() {
		const Database &db = *reinterpret_cast<const Database *>(1);
		const SongLoader secure_loader(&db, storage);
		const SongLoader insecure_loader(*reinterpret_cast<const Client *>(1),
						 &db, storage);

		/* map to music_directory */
		DetachedSong song1("bar.ogg", MakeTag2b());
		auto s1 = ToString(song1);
		auto se = ToString(DetachedSong(uri2, MakeTag2c()));
		CPPUNIT_ASSERT(playlist_check_translate_song(song1, "/music/foo",
							     insecure_loader));
		CPPUNIT_ASSERT_EQUAL(se, ToString(song1));

		/* illegal because secure=false */
		DetachedSong song2("bar.ogg", MakeTag2b());
		CPPUNIT_ASSERT(!playlist_check_translate_song(song1, "/foo",
							      insecure_loader));

		/* legal because secure=true */
		DetachedSong song3("bar.ogg", MakeTag1b());
		s1 = ToString(song3);
		se = ToString(DetachedSong(uri1, MakeTag1c()));
		CPPUNIT_ASSERT(playlist_check_translate_song(song3, "/foo",
							     secure_loader));
		CPPUNIT_ASSERT_EQUAL(se, ToString(song3));

		/* relative to http:// */
		DetachedSong song4("bar.ogg", MakeTag2a());
		s1 = ToString(song4);
		se = ToString(DetachedSong("http://example.com/foo/bar.ogg", MakeTag2a()));
		CPPUNIT_ASSERT(playlist_check_translate_song(song4, "http://example.com/foo",
							     insecure_loader));
		CPPUNIT_ASSERT_EQUAL(se, ToString(song4));
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(TranslateSongTest);

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	storage = CreateLocalStorage(Path::FromFS(music_directory));

	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
