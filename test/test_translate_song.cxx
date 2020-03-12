/*
 * Unit tests for playlist_check_translate_song().
 */

#include "MakeTag.hxx"
#include "playlist/PlaylistSong.hxx"
#include "song/DetachedSong.hxx"
#include "SongLoader.hxx"
#include "client/Client.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "util/Domain.hxx"
#include "fs/AllocatedPath.hxx"
#include "ls.hxx"
#include "Log.hxx"
#include "db/DatabaseSong.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/plugins/LocalStorage.hxx"
#include "Mapper.hxx"
#include "time/ChronoUtil.hxx"

#include <gtest/gtest.h>

#include <string.h>
#include <stdio.h>

void
Log(LogLevel, const Domain &domain, const char *msg) noexcept
{
	fprintf(stderr, "[%s] %s\n", domain.GetName(), msg);
}

bool
uri_supported_scheme(const char *uri) noexcept
{
	return strncmp(uri, "http://", 7) == 0;
}

static constexpr auto music_directory = PATH_LITERAL("/music");
static Storage *storage;

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

DetachedSong
DatabaseDetachSong([[maybe_unused]] const Database &db,
		   [[maybe_unused]] const Storage *_storage,
		   const char *uri)
{
	if (strcmp(uri, uri2) == 0)
		return DetachedSong(uri, MakeTag2a());

	throw std::runtime_error("No such song");
}

bool
DetachedSong::LoadFile(Path path)
{
	if (path.ToUTF8() == uri1) {
		SetTag(MakeTag1a());
		return true;
	}

	return false;
}

const Database *
Client::GetDatabase() const noexcept
{
	return reinterpret_cast<const Database *>(this);
}

const Storage *
Client::GetStorage() const noexcept
{
	return ::storage;
}

void
Client::AllowFile([[maybe_unused]] Path path_fs) const
{
	/* always fail, so a SongLoader with a non-nullptr
	   Client pointer will be regarded "insecure", while one with
	   client==nullptr will allow all files */
	throw std::runtime_error("foo");
}

static std::string
ToString(const Tag &tag)
{
	std::string result;

	if (!tag.duration.IsNegative())
		result.append(std::to_string(tag.duration.ToMS()));

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

	if (!IsNegative(song.GetLastModified()))
		result.append(std::to_string(std::chrono::system_clock::to_time_t(song.GetLastModified())));

	result.push_back('|');

	if (song.GetStartTime().IsPositive())
		result.append(std::to_string(song.GetStartTime().ToMS()));

	result.push_back('-');

	if (song.GetEndTime().IsPositive())
		result.append(std::to_string(song.GetEndTime().ToMS()));

	result.push_back('|');

	result.append(ToString(song.GetTag()));

	return result;
}

class TranslateSongTest : public ::testing::Test {
	std::unique_ptr<Storage> _storage;

protected:
	void SetUp() override {
		_storage = CreateLocalStorage(Path::FromFS(music_directory));
		storage = _storage.get();
	}

	void TearDown() override {
		_storage.reset();
	}
};

TEST_F(TranslateSongTest, AbsoluteURI)
{
	DetachedSong song1("http://example.com/foo.ogg");
	auto se = ToString(song1);
	const SongLoader loader(nullptr, nullptr);
	EXPECT_TRUE(playlist_check_translate_song(song1, "/ignored",
						  loader));
	EXPECT_EQ(se, ToString(song1));
}

TEST_F(TranslateSongTest, Insecure)
{
	/* illegal because secure=false */
	DetachedSong song1 (uri1);
	const SongLoader loader(*reinterpret_cast<const Client *>(1));
	EXPECT_FALSE(playlist_check_translate_song(song1, {},
						   loader));
}

TEST_F(TranslateSongTest, Secure)
{
	DetachedSong song1(uri1, MakeTag1b());
	auto se = ToString(DetachedSong(uri1, MakeTag1c()));

	const SongLoader loader(nullptr, nullptr);
	EXPECT_TRUE(playlist_check_translate_song(song1, "/ignored",
						  loader));
	EXPECT_EQ(se, ToString(song1));
}

TEST_F(TranslateSongTest, InDatabase)
{
	const SongLoader loader(reinterpret_cast<const Database *>(1),
				storage);

	DetachedSong song1("doesntexist");
	EXPECT_FALSE(playlist_check_translate_song(song1, {},
						   loader));

	DetachedSong song2(uri2, MakeTag2b());
	auto se = ToString(DetachedSong(uri2, MakeTag2c()));
	EXPECT_TRUE(playlist_check_translate_song(song2, {},
						  loader));
	EXPECT_EQ(se, ToString(song2));

	DetachedSong song3("/music/foo/bar.ogg", MakeTag2b());
	se = ToString(DetachedSong(uri2, MakeTag2c()));
	EXPECT_TRUE(playlist_check_translate_song(song3, {},
						  loader));
	EXPECT_EQ(se, ToString(song3));
}

TEST_F(TranslateSongTest, Relative)
{
	const Database &db = *reinterpret_cast<const Database *>(1);
	const SongLoader secure_loader(&db, storage);
	const SongLoader insecure_loader(*reinterpret_cast<const Client *>(1),
					 &db, storage);

	/* map to music_directory */
	DetachedSong song1("bar.ogg", MakeTag2b());
	auto se = ToString(DetachedSong(uri2, MakeTag2c()));
	EXPECT_TRUE(playlist_check_translate_song(song1, "/music/foo",
						  insecure_loader));
	EXPECT_EQ(se, ToString(song1));

	/* illegal because secure=false */
	DetachedSong song2("bar.ogg", MakeTag2b());
	EXPECT_FALSE(playlist_check_translate_song(song1, "/foo",
						   insecure_loader));

	/* legal because secure=true */
	DetachedSong song3("bar.ogg", MakeTag1b());
	se = ToString(DetachedSong(uri1, MakeTag1c()));
	EXPECT_TRUE(playlist_check_translate_song(song3, "/foo",
						  secure_loader));
	EXPECT_EQ(se, ToString(song3));

	/* relative to http:// */
	DetachedSong song4("bar.ogg", MakeTag2a());
	se = ToString(DetachedSong("http://example.com/foo/bar.ogg", MakeTag2a()));
	EXPECT_TRUE(playlist_check_translate_song(song4, "http://example.com/foo",
						  insecure_loader));
	EXPECT_EQ(se, ToString(song4));
}

TEST_F(TranslateSongTest, Backslash)
{
	const SongLoader loader(reinterpret_cast<const Database *>(1),
				storage);

	DetachedSong song1("foo\\bar.ogg", MakeTag2b());
#ifdef _WIN32
	/* on Windows, all backslashes are converted to slashes in
	   relative paths from playlists */
	auto se = ToString(DetachedSong(uri2, MakeTag2c()));
	EXPECT_TRUE(playlist_check_translate_song(song1, {},
						  loader));
	EXPECT_EQ(se, ToString(song1));
#else
	/* backslash only supported on Windows */
	EXPECT_FALSE(playlist_check_translate_song(song1, {},
						   loader));
#endif
}
