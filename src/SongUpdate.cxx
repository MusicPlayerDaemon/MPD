/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
Song::LoadFile(Storage &storage, const char *path_utf8, Directory &parent)
{
	assert(!uri_has_scheme(path_utf8));
	assert(std::strchr(path_utf8, '\n') == nullptr);

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
Song::LoadFromArchive(ArchiveFile &archive, const char *name_utf8,
		      Directory &parent) noexcept
{
	assert(!uri_has_scheme(name_utf8));
	assert(std::strchr(name_utf8, '\n') == nullptr);

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
