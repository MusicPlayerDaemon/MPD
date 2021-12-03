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

#ifndef MPD_TEXT_FILE_HXX
#define MPD_TEXT_FILE_HXX

#include "io/LineReader.hxx"
#include "config.h"

#include <memory>

class Path;
class FileReader;
class AutoGunzipReader;
class BufferedReader;

class TextFile final : public LineReader {
	const std::unique_ptr<FileReader> file_reader;

#ifdef ENABLE_ZLIB
	const std::unique_ptr<AutoGunzipReader> gunzip_reader;
#endif

	const std::unique_ptr<BufferedReader> buffered_reader;

public:
	explicit TextFile(Path path_fs);

	TextFile(const TextFile &other) = delete;

	~TextFile() noexcept;

	/* virtual methods from class LineReader */
	char *ReadLine() override;
};

#endif
