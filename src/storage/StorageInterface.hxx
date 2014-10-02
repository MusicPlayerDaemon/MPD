// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

#include <memory>
#include <string>
#include <string_view>

struct StorageFileInfo;
class AllocatedPath;

class StorageDirectoryReader {
public:
	StorageDirectoryReader() = default;
	StorageDirectoryReader(const StorageDirectoryReader &) = delete;
	virtual ~StorageDirectoryReader() noexcept = default;

	[[nodiscard]]
	virtual const char *Read() noexcept = 0;

	/**
	 * Throws #std::runtime_error on error.
	 */
	[[nodiscard]]
	virtual StorageFileInfo GetInfo(bool follow) = 0;
};

class Storage {
public:
	Storage() = default;
	Storage(const Storage &) = delete;
	virtual ~Storage() noexcept = default;

	/**
	 * Throws #std::runtime_error on error.
	 */
	[[nodiscard]]
	virtual StorageFileInfo GetInfo(std::string_view uri_utf8, bool follow) = 0;

	/**
	 * Throws #std::runtime_error on error.
	 */
	[[nodiscard]]
	virtual std::unique_ptr<StorageDirectoryReader> OpenDirectory(std::string_view uri_utf8) = 0;

	/**
	 * Map the given relative URI to an absolute URI.
	 */
	[[nodiscard]] [[gnu::pure]]
	virtual std::string MapUTF8(std::string_view uri_utf8) const noexcept = 0;

	/**
	 * Map the given relative URI to a local file path.  Returns
	 * nullptr on error or if this storage does not
	 * support local files.
	 */
	[[nodiscard]] [[gnu::pure]]
	virtual AllocatedPath MapFS(std::string_view uri_utf8) const noexcept;

	[[nodiscard]] [[gnu::pure]]
	AllocatedPath MapChildFS(std::string_view uri_utf8,
				 std::string_view child_utf8) const noexcept;

	/**
	 * Check if the given URI points inside this storage.  If yes,
	 * then it returns a relative URI (pointing inside the given
	 * string); if not, returns nullptr.
	 */
	[[nodiscard]] [[gnu::pure]]
	virtual std::string_view MapToRelativeUTF8(std::string_view uri_utf8) const noexcept = 0;

	/**
	 * Open a file in this storage as an #InputStream.
	 *
	 * Throws on error
	 */
	[[nodiscard]]
	virtual InputStreamPtr OpenFile(std::string_view uri_utf8, Mutex &mutex) = 0;
};
