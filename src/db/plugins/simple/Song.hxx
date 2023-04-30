// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_HXX
#define MPD_SONG_HXX

#include "Ptr.hxx"
#include "Chrono.hxx"
#include "tag/Tag.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/IntrusiveList.hxx"
#include "config.h"

#include <string>

struct Directory;
class ExportedSong;
class DetachedSong;
class Storage;
class ArchiveFile;

/**
 * A song file inside the configured music directory.  Internal
 * #SimpleDatabase class.
 */
struct Song : IntrusiveListHook<> {
	/* Note: the #IntrusiveListHook is protected with the global
	   #db_mutex.  Read access in the update thread does not need
	   protection. */

	/**
	 * The #Directory that contains this song.
	 */
	Directory &parent;

	/**
	 * The file name.
	 */
	std::string filename;

	/**
	 * If non-empty, then this object does not describe a file
	 * within the `music_directory`, but some sort of symbolic
	 * link pointing to this value.  It can be an absolute URI
	 * (i.e. with URI scheme) or a URI relative to this object
	 * (which may begin with one or more "../").
	 */
	std::string target;

	Tag tag;

	/**
	 * The time stamp of the last file modification.  A negative
	 * value means that this is unknown/unavailable.
	 */
	std::chrono::system_clock::time_point mtime =
		std::chrono::system_clock::time_point::min();

	/**
	 * Start of this sub-song within the file.
	 */
	SongTime start_time = SongTime::zero();

	/**
	 * End of this sub-song within the file.
	 * Unused if zero.
	 */
	SongTime end_time = SongTime::zero();

	/**
	 * The audio format of the song, if given by the decoder
	 * plugin.  May be undefined if unknown.
	 */
	AudioFormat audio_format = AudioFormat::Undefined();

	/**
	 * Is this song referenced by at least one playlist file that
	 * is part of the database?
	 */
	bool in_playlist = false;

	template<typename F>
	Song(F &&_filename, Directory &_parent) noexcept
		:parent(_parent), filename(std::forward<F>(_filename)) {}

	Song(DetachedSong &&other, Directory &_parent) noexcept;

	[[gnu::pure]]
	const char *GetFilenameSuffix() const noexcept;

	/**
	 * Checks whether the decoder plugin for this song is
	 * available.
	 */
	[[gnu::pure]]
	bool IsPluginAvailable() const noexcept;

	/**
	 * allocate a new song structure with a local file name and attempt to
	 * load its metadata.  If all decoder plugin fail to read its meta
	 * data, nullptr is returned.
	 *
	 * Throws on error.
	 *
	 * @return the song on success, nullptr if the file was not
	 * recognized
	 */
	static SongPtr LoadFile(Storage &storage, std::string_view name_utf8,
				Directory &parent);

	/**
	 * Throws on error.
	 *
	 * @return true on success, false if the file was not recognized
	 */
	bool UpdateFile(Storage &storage);

#ifdef ENABLE_ARCHIVE
	static SongPtr LoadFromArchive(ArchiveFile &archive,
				       std::string_view name_utf8,
				       Directory &parent) noexcept;
	bool UpdateFileInArchive(ArchiveFile &archive) noexcept;
#endif

	/**
	 * Returns the URI of the song in UTF-8 encoding, including its
	 * location within the music directory.
	 */
	[[gnu::pure]]
	std::string GetURI() const noexcept;

	[[gnu::pure]]
	ExportedSong Export() const noexcept;
};

#endif
