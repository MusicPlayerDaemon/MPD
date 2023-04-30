// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h" /* must be first for large file support */
#include "song/DetachedSong.hxx"
#include "db/plugins/simple/Song.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "decoder/DecoderList.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileInfo.hxx"
#include "tag/Builder.hxx"
#include "TagFile.hxx"
#include "TagStream.hxx"
#include "util/UriExtract.hxx"

#ifdef ENABLE_ARCHIVE
#include "TagArchive.hxx"
#endif

#include <cassert>

#include <string.h>

#ifdef ENABLE_DATABASE

bool
Song::IsPluginAvailable() const noexcept
{
	const char *suffix = GetFilenameSuffix();
	return suffix != nullptr &&
		decoder_plugins_supports_suffix(suffix);
}

SongPtr
Song::LoadFile(Storage &storage, std::string_view path_utf8, Directory &parent)
{
	assert(!uri_has_scheme(path_utf8));
	assert(path_utf8.find('\n') == path_utf8.npos);

	auto song = std::make_unique<Song>(path_utf8, parent);
	if (!song->UpdateFile(storage))
		return nullptr;

	return song;
}

#endif

#ifdef ENABLE_DATABASE

bool
Song::UpdateFile(Storage &storage)
{
	const auto &relative_uri = GetURI();

	const auto info = storage.GetInfo(relative_uri.c_str(), true);
	if (!info.IsRegular())
		return false;

	TagBuilder tag_builder;
	auto new_audio_format = AudioFormat::Undefined();

	try {
		const auto path_fs = storage.MapFS(relative_uri.c_str());
		if (path_fs.IsNull()) {
			const auto absolute_uri =
				storage.MapUTF8(relative_uri.c_str());
			if (!tag_stream_scan(absolute_uri.c_str(), tag_builder,
					     &new_audio_format))
				return false;
		} else {
			if (!ScanFileTagsWithGeneric(path_fs, tag_builder,
						     &new_audio_format))
				return false;
		}
	} catch (...) {
		// TODO: log or propagate I/O errors?
		return false;
	}

	mtime = info.mtime;
	audio_format = new_audio_format;
	tag_builder.Commit(tag);
	return true;
}

#ifdef ENABLE_ARCHIVE

SongPtr
Song::LoadFromArchive(ArchiveFile &archive, std::string_view name_utf8,
		      Directory &parent) noexcept
{
	assert(!uri_has_scheme(name_utf8));
	assert(name_utf8.find('\n') == name_utf8.npos);

	auto song = std::make_unique<Song>(name_utf8, parent);
	if (!song->UpdateFileInArchive(archive))
		return nullptr;

	return song;
}

bool
Song::UpdateFileInArchive(ArchiveFile &archive) noexcept
{
	assert(parent.device == DEVICE_INARCHIVE);

	std::string path_utf8(filename);

	for (const Directory *directory = &parent;
	     directory->parent != nullptr &&
		     directory->parent->device == DEVICE_INARCHIVE;
	     directory = directory->parent) {
		path_utf8.insert(path_utf8.begin(), '/');
		path_utf8.insert(0, directory->GetName());
	}

	TagBuilder tag_builder;
	if (!tag_archive_scan(archive, path_utf8.c_str(), tag_builder))
		return false;

	tag_builder.Commit(tag);
	return true;
}

#endif

#endif /* ENABLE_DATABASE */

bool
DetachedSong::LoadFile(Path path)
{
	const FileInfo fi(path);
	if (!fi.IsRegular())
		return false;

	TagBuilder tag_builder;
	auto new_audio_format = AudioFormat::Undefined();

	try {
		if (!ScanFileTagsWithGeneric(path, tag_builder, &new_audio_format))
			return false;
	} catch (...) {
		// TODO: log or propagate I/O errors?
		return false;
	}

	mtime = fi.GetModificationTime();
	audio_format = new_audio_format;
	tag_builder.Commit(tag);
	return true;
}

bool
DetachedSong::Update()
{
	if (IsAbsoluteFile()) {
		const AllocatedPath path_fs =
			AllocatedPath::FromUTF8Throw(GetRealURI());

		return LoadFile(path_fs);
	} else if (IsRemote()) {
		TagBuilder tag_builder;
		auto new_audio_format = AudioFormat::Undefined();

		try {
			if (!tag_stream_scan(uri.c_str(), tag_builder,
					     &new_audio_format))
				return false;
		} catch (...) {
			// TODO: log or propagate I/O errors?
			return false;
		}

		mtime = std::chrono::system_clock::time_point::min();
		audio_format = new_audio_format;
		tag_builder.Commit(tag);
		return true;
	} else
		// TODO: implement
		return false;
}
