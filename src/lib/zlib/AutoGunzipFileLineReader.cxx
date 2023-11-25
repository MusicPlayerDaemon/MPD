// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AutoGunzipFileLineReader.hxx"
#include "io/FileReader.hxx"
#include "io/BufferedReader.hxx"
#include "lib/zlib/AutoGunzipReader.hxx"
#include "fs/Path.hxx"

#include <cassert>

AutoGunzipFileLineReader::AutoGunzipFileLineReader(Path path_fs)
	:file_reader(std::make_unique<FileReader>(path_fs)),
#ifdef ENABLE_ZLIB
	 gunzip_reader(std::make_unique<AutoGunzipReader>(*file_reader)),
#endif
	 buffered_reader(std::make_unique<BufferedReader>(*
#ifdef ENABLE_ZLIB
							  gunzip_reader
#else
							  file_reader
#endif
							  ))
{
}

AutoGunzipFileLineReader::~AutoGunzipFileLineReader() noexcept = default;

char *
AutoGunzipFileLineReader::ReadLine()
{
	assert(buffered_reader != nullptr);

	return buffered_reader->ReadLine();
}
