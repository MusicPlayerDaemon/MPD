// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "config.h"

#ifdef ENABLE_ZLIB

#include "io/LineReader.hxx"

#include <memory>

class Path;
class FileReader;
class AutoGunzipReader;
class BufferedReader;

class AutoGunzipFileLineReader final : public LineReader {
	const std::unique_ptr<FileReader> file_reader;

	const std::unique_ptr<AutoGunzipReader> gunzip_reader;

	const std::unique_ptr<BufferedReader> buffered_reader;

public:
	explicit AutoGunzipFileLineReader(Path path_fs);

	AutoGunzipFileLineReader(const AutoGunzipFileLineReader &other) = delete;

	~AutoGunzipFileLineReader() noexcept;

	/* virtual methods from class LineReader */
	char *ReadLine() override;
};

#else

#include "io/FileLineReader.hxx"

using AutoGunzipFileLineReader = FileLineReader;

#endif
