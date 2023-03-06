// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
