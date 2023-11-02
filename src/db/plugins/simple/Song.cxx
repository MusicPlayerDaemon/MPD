// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Song.hxx"
#include "ExportedSong.hxx"
#include "Directory.hxx"
#include "tag/Tag.hxx"
#include "tag/Builder.hxx"
#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"
#include "fs/Traits.hxx"
#include "time/ChronoUtil.hxx"
#include "util/IterableSplitString.hxx"

using std::string_view_literals::operator""sv;

Song::Song(DetachedSong &&other, Directory &_parent) noexcept
	:parent(_parent),
	 filename(other.GetURI()),
	 tag(std::move(other.WritableTag())),
	 mtime(other.GetLastModified()),
	 added(other.GetAdded()),
	 start_time(other.GetStartTime()),
	 end_time(other.GetEndTime()),
	 audio_format(other.GetAudioFormat())
{
}

const char *
Song::GetFilenameSuffix() const noexcept
{
	return target.empty()
		? PathTraitsUTF8::GetFilenameSuffix(filename.c_str())
		: PathTraitsUTF8::GetPathSuffix(target.c_str());
}

std::string
Song::GetURI() const noexcept
{
	if (parent.IsRoot())
		return filename;
	else {
		const char *path = parent.GetPath();
		return PathTraitsUTF8::Build(path, filename);
	}
}

/**
 * Path name traversal of a #Directory.
 */
[[gnu::pure]]
static const Directory *
FindTargetDirectory(const Directory &base, std::string_view path) noexcept
{
	const auto *directory = &base;
	for (const std::string_view name : IterableSplitString(path, '/')) {
		if (name.empty() || name == "."sv)
			continue;

		directory = name == ".."sv
			? directory->parent
			: directory->FindChild(name);
		if (directory == nullptr)
			break;
	}

	return directory;
}

/**
 * Path name traversal of a #Song.
 */
[[gnu::pure]]
static const Song *
FindTargetSong(const Directory &_directory, std::string_view target) noexcept
{
	auto [path, last] = SplitLast(target, '/');
	if (last.data() == nullptr) {
		last = path;
		path = {};
	}

	if (last.empty())
		return nullptr;

	const auto *directory = FindTargetDirectory(_directory, path);
	if (directory == nullptr)
		return nullptr;

	return directory->FindSong(last);
}

ExportedSong
Song::Export() const noexcept
{
	const auto *target_song = !target.empty()
		? FindTargetSong(parent, target)
		: nullptr;

	Tag merged_tag;
	if (target_song != nullptr) {
		/* if we found the target song (which may be the
		   underlying song file of a CUE file), merge the tags
		   from that song with this song's tags (from the CUE
		   file) */
		TagBuilder builder(tag);
		builder.Complement(target_song->tag);
		merged_tag = builder.Commit();
	}

	ExportedSong dest = merged_tag.IsDefined()
		? ExportedSong(filename.c_str(), std::move(merged_tag))
		: ExportedSong(filename.c_str(), tag);
	if (!parent.IsRoot())
		dest.directory = parent.GetPath();
	if (!target.empty())
		dest.real_uri = target.c_str();
	dest.mtime = IsNegative(mtime) && target_song != nullptr
		? target_song->mtime
		: mtime;
	dest.added = IsNegative(added) && target_song != nullptr
		? target_song->added
		: added;
	dest.start_time = start_time.IsZero() && target_song != nullptr
		? target_song->start_time
		: start_time;
	dest.end_time = end_time.IsZero() && target_song != nullptr
		? target_song->end_time
		: end_time;
	dest.audio_format = audio_format.IsDefined() || target_song == nullptr
		? audio_format
		: target_song->audio_format;
	return dest;
}
