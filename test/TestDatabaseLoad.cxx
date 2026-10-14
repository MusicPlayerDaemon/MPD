// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "db/plugins/simple/DatabaseSave.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "io/LineReader.hxx"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>

class StringLineReader final : public LineReader {
	std::string buffer;
	std::size_t position = 0;

public:
	explicit StringLineReader(std::string_view _buffer) noexcept
		:buffer(_buffer) {}

	char *ReadLine() noexcept override {
		if (position >= buffer.size())
			return nullptr;

		char *line = buffer.data() + position;
		const auto newline = buffer.find('\n', position);
		if (newline == buffer.npos) {
			position = buffer.size();
		} else {
			buffer[newline] = 0;
			position = newline + 1;
		}

		return line;
	}
};

static void
Load(std::string_view text)
{
	Directory root{{}, nullptr};
	StringLineReader reader{text};
	db_load_internal(reader, root, true);
}

static constexpr std::string_view header =
	"info_begin\n"
	"format: 2\n"
	"info_end\n";

static constexpr std::string_view body =
	"song_begin: a.mp3\n"
	"Title: A\n"
	"song_end\n"
	"directory: sub\n"
	"begin: sub\n"
	"song_begin: b.mp3\n"
	"Title: B\n"
	"song_end\n"
	"end: sub\n";

TEST(DatabaseLoad, Complete)
{
	EXPECT_NO_THROW(Load(std::string{header} + std::string{body}));
}

TEST(DatabaseLoad, TruncatedRoot)
{
	/* the root directory has no "end:" line, so the end of file
	   after a complete song is not an error */
	EXPECT_NO_THROW(Load(std::string{header} +
			     "song_begin: a.mp3\n"
			     "song_end\n"));
}

TEST(DatabaseLoad, TruncatedSong)
{
	EXPECT_THROW(Load(std::string{header} +
			  "song_begin: a.mp3\n"
			  "Title: A\n"),
		     std::runtime_error);
}

TEST(DatabaseLoad, TruncatedSubdirectory)
{
	EXPECT_THROW(Load(std::string{header} +
			  "directory: sub\n"
			  "begin: sub\n"
			  "song_begin: b.mp3\n"
			  "song_end\n"),
		     std::runtime_error);
}
