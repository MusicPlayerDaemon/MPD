// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TextFile.hxx"
#include "io/FileReader.hxx"
#include "io/BufferedReader.hxx"
#include "lib/zlib/AutoGunzipReader.hxx"
#include "fs/Path.hxx"

#include <cassert>

TextFile::TextFile(Path path_fs)
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

TextFile::~TextFile() noexcept = default;

char *
TextFile::ReadLine()
{
	assert(buffered_reader != nullptr);

	return buffered_reader->ReadLine();
}
