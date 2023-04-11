// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DirectorySave.hxx"
#include "Directory.hxx"
#include "Song.hxx"
#include "SongSave.hxx"
#include "song/DetachedSong.hxx"
#include "PlaylistDatabase.hxx"
#include "io/LineReader.hxx"
#include "io/BufferedOutputStream.hxx"
#include "time/ChronoUtil.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/NumberParser.hxx"

#include <fmt/format.h>

#include <string.h>

#define DIRECTORY_DIR "directory: "
#define DIRECTORY_TYPE "type: "
#define DIRECTORY_MTIME "mtime: "
#define DIRECTORY_BEGIN "begin: "
#define DIRECTORY_END "end: "

[[gnu::const]]
static const char *
DeviceToTypeString(unsigned device) noexcept
{
	switch (device) {
	case DEVICE_INARCHIVE:
		return "archive";

	case DEVICE_CONTAINER:
		return "container";

	case DEVICE_PLAYLIST:
		return "playlist";

	default:
		return nullptr;
	}
}

[[gnu::pure]]
static unsigned
ParseTypeString(const char *type) noexcept
{
	if (StringIsEqual(type, "archive"))
		return DEVICE_INARCHIVE;
	else if (StringIsEqual(type, "container"))
		return DEVICE_CONTAINER;
	else if (StringIsEqual(type, "playlist"))
		return DEVICE_PLAYLIST;
	else
		return 0;
}

void
directory_save(BufferedOutputStream &os, const Directory &directory)
{
	if (!directory.IsRoot()) {
		const char *type = DeviceToTypeString(directory.device);
		if (type != nullptr)
			os.Fmt(FMT_STRING(DIRECTORY_TYPE "{}\n"), type);

		if (!IsNegative(directory.mtime))
			os.Fmt(FMT_STRING(DIRECTORY_MTIME "{}\n"),
			       std::chrono::system_clock::to_time_t(directory.mtime));

		os.Fmt(FMT_STRING(DIRECTORY_BEGIN "{}\n"), directory.GetPath());
	}

	for (const auto &child : directory.children) {
		if (child.IsMount())
			continue;

		os.Fmt(FMT_STRING(DIRECTORY_DIR "{}\n"), child.GetName());
		directory_save(os, child);
	}

	for (const auto &song : directory.songs)
		song_save(os, song);

	playlist_vector_save(os, directory.playlists);

	if (!directory.IsRoot())
		os.Fmt(FMT_STRING(DIRECTORY_END "{}\n"), directory.GetPath());
}

static bool
ParseLine(Directory &directory, const char *line)
{
	const char *p;
	if ((p = StringAfterPrefix(line, DIRECTORY_MTIME))) {
		const auto mtime = ParseUint64(p);
		if (mtime > 0)
			directory.mtime = std::chrono::system_clock::from_time_t(mtime);
	} else if ((p = StringAfterPrefix(line, DIRECTORY_TYPE))) {
		directory.device = ParseTypeString(p);
	} else
		return false;

	return true;
}

static Directory *
directory_load_subdir(LineReader &file, Directory &parent, std::string_view name)
{
	if (parent.FindChild(name) != nullptr)
		throw FmtRuntimeError("Duplicate subdirectory '{}'", name);

	Directory *directory = parent.CreateChild(name);

	try {
		while (true) {
			const char *line = file.ReadLine();
			if (line == nullptr)
				throw std::runtime_error("Unexpected end of file");

			if (StringStartsWith(line, DIRECTORY_BEGIN))
				break;

			if (!ParseLine(*directory, line))
				throw FmtRuntimeError("Malformed line: {}", line);
		}

		directory_load(file, *directory);
	} catch (...) {
		directory->Delete();
		throw;
	}

	return directory;
}

void
directory_load(LineReader &file, Directory &directory)
{
	const char *line;

	while ((line = file.ReadLine()) != nullptr &&
	       !StringStartsWith(line, DIRECTORY_END)) {
		const char *p;
		if ((p = StringAfterPrefix(line, DIRECTORY_DIR))) {
			directory_load_subdir(file, directory, p);
		} else if ((p = StringAfterPrefix(line, SONG_BEGIN))) {
			const char *name = p;

			if (directory.FindSong(name) != nullptr)
				throw FmtRuntimeError("Duplicate song '{}'",
						      name);

			std::string target;
			bool in_playlist = false;
			auto detached_song = song_load(file, name,
						       &target, &in_playlist);

			auto song = std::make_unique<Song>(std::move(detached_song),
							   directory);
			song->target = std::move(target);
			song->in_playlist = in_playlist;

			directory.AddSong(std::move(song));
		} else if ((p = StringAfterPrefix(line, PLAYLIST_META_BEGIN))) {
			const char *name = p;
			playlist_metadata_load(file, directory.playlists, name);
		} else {
			throw FmtRuntimeError("Malformed line: {}", line);
		}
	}
}
